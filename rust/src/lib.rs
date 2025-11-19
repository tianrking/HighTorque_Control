//! LivelyBot High Torque Motor Control Library
//!
//! High-performance Rust implementation for controlling LivelyBot motors via CAN bus.
//! Supports motor scanning, velocity control, and angle stream control.

use anyhow::{Result, anyhow};
use socketcan::{CanSocket, CanFrame, CanId, Socket, EmbeddedFrame};
use std::time::Duration;
use std::thread;

// Protocol coefficients
pub const FACTOR_POS: f64 = 10000.0;    // 1圈 = 10000
pub const FACTOR_VEL: f64 = 4000.0;     // 1r/s = 4000
pub const FACTOR_ACC: f64 = 1000.0;     // 1r/s² = 1000
pub const FACTOR_TQE: f64 = 200.0;      // 通用电机系数
pub const MAGIC_POS: i16 = -32768;      // 0x8000 (Int16 Min) -> 代表"无位置限制"

#[derive(Debug, Clone)]
pub struct MotorInfo {
    pub motor_id: u8,
    pub is_online: bool,
    pub name: String,
    pub hardware_version: String,
    pub response_time_ms: u64,
}

impl Default for MotorInfo {
    fn default() -> Self {
        Self {
            motor_id: 0,
            is_online: false,
            name: "Unknown".to_string(),
            hardware_version: "Unknown".to_string(),
            response_time_ms: 0,
        }
    }
}

/// LivelyBot motor controller using CAN interface
pub struct LivelyMotorController {
    socket: CanSocket,
    channel: String,
    bitrate: u32,
}

impl LivelyMotorController {
    /// Create a new motor controller
    pub fn new(channel: &str, bitrate: u32) -> Result<Self> {
        let socket = CanSocket::open(channel)?;

        Ok(Self {
            socket,
            channel: channel.to_string(),
            bitrate,
        })
    }

    /// Send a CAN frame
    pub fn send_frame(&self, id: u32, data: &[u8]) -> Result<()> {
        let can_id = CanId::extended(id).ok_or(anyhow!("Invalid CAN ID"))?;
        let frame = CanFrame::new(can_id, data).ok_or(anyhow!("Failed to create CAN frame"))?;
        self.socket.write_frame(&frame)?;
        Ok(())
    }

    /// Read a CAN frame with timeout
    pub fn read_frame_with_timeout(&self, timeout_ms: u64) -> Result<Option<CanFrame>> {
        self.socket.set_read_timeout(Duration::from_millis(timeout_ms))?;
        match self.socket.read_frame() {
            Ok(frame) => Ok(Some(frame)),
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => Ok(None),
            Err(e) => Err(e.into()),
        }
    }

    /// Ping a motor to check if it's online
    pub fn ping_motor(&self, motor_id: u8) -> Result<MotorInfo> {
        let start_time = std::time::Instant::now();
        let mut info = MotorInfo {
            motor_id,
            ..Default::default()
        };

        // Send ping command: 0x8000 | motor_id with CAN_EFF_FLAG
        let ping_id = 0x8000u32 | motor_id as u32;
        let ping_data = [0x11, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50];

        self.send_frame(ping_id, &ping_data)?;
        thread::sleep(Duration::from_millis(10));

        // Wait for response
        let timeout_start = std::time::Instant::now();
        while timeout_start.elapsed().as_millis() < 50 {
            if let Some(frame) = self.read_frame_with_timeout(10)? {
                let can_id = frame.id();

                // Parse response (same logic as Python/C++ versions)
                let (source_id, direct_id) = match can_id {
                    socketcan::Id::Standard(id) => {
                        let id_raw = id.as_raw();
                        (((id_raw >> 8) & 0x7F) as u8, (id_raw & 0xFF) as u8)
                    }
                    socketcan::Id::Extended(id) => {
                        let id_raw = id.as_raw();
                        (((id_raw >> 8) & 0x7F) as u8, (id_raw & 0xFF) as u8)
                    }
                };

                let detected_id = if source_id > 0 && source_id < 128 {
                    source_id
                } else if direct_id == motor_id {
                    direct_id
                } else {
                    continue;
                };

                if detected_id == motor_id {
                    info.response_time_ms = start_time.elapsed().as_millis() as u64;
                    info.is_online = true;

                    // Parse motor info from response
                    let data = frame.data();
                    if data.len() >= 4 && data[0] == 0x51 {
                        let mut name_bytes = [0u8; 3];
                        name_bytes.copy_from_slice(&data[1..4]);
                        if let Ok(name) = std::str::from_utf8(&name_bytes) {
                            info.name = name.trim_end_matches('\0').to_string();
                        }
                    }

                    if data.len() >= 8 {
                        let mut version_bytes = [0u8; 4];
                        version_bytes.copy_from_slice(&data[4..8]);
                        if let Ok(version) = std::str::from_utf8(&version_bytes) {
                            info.hardware_version = version.trim_end_matches('\0').to_string();
                        }
                    }

                    break;
                }
            }
        }

        Ok(info)
    }

    /// Scan a range of motor IDs
    pub fn scan_range(&self, start_id: u8, end_id: u8) -> Result<Vec<MotorInfo>> {
        let mut motors = Vec::new();

        for motor_id in start_id..=end_id {
            let info = self.ping_motor(motor_id)?;
            motors.push(info);
            thread::sleep(Duration::from_millis(10));
        }

        Ok(motors)
    }

    /// Enable motor (position mode)
    pub fn enable_motor(&self, motor_id: u8) -> Result<()> {
        let motor_id = motor_id as u32;

        // Set mode to 0x0A (Position Mode)
        let mode_data = [0x01, 0x00, 0x0A, 0x50, 0x50, 0x50, 0x50, 0x50];
        self.send_frame(motor_id, &mode_data)?;
        thread::sleep(Duration::from_millis(50));

        // Set PID parameters
        let kp_data = {
            let mut data = [0x0D, 0x23, 0x00, 0x00, 0x00, 0x00, 0x50, 0x50];
            let kp = 1.0f32;
            data[2..6].copy_from_slice(&kp.to_le_bytes());
            data
        };
        self.send_frame(motor_id, &kp_data)?;
        thread::sleep(Duration::from_millis(20));

        let kd_data = {
            let mut data = [0x0D, 0x24, 0x00, 0x00, 0x00, 0x00, 0x50, 0x50];
            let kd = 0.1f32;
            data[2..6].copy_from_slice(&kd.to_le_bytes());
            data
        };
        self.send_frame(motor_id, &kd_data)?;

        Ok(())
    }

    /// Disable motor
    pub fn disable_motor(&self, motor_id: u8) -> Result<()> {
        let data = [0x01, 0x00, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50];
        self.send_frame(motor_id as u32, &data)
    }

    /// Send velocity control command (0xAD)
    pub fn send_velocity_command(&self, position: i16, velocity: i16, acceleration: i16) -> Result<()> {
        let mut data = [0u8; 8];
        data[0..2].copy_from_slice(&position.to_le_bytes());
        data[2..4].copy_from_slice(&velocity.to_le_bytes());
        data[4..6].copy_from_slice(&acceleration.to_le_bytes());
        data[6] = 0x50;
        data[7] = 0x50;

        self.send_frame(0x00AD, &data)
    }

    /// Send angle stream control command (0x90)
    pub fn send_angle_command(&self, angle: i16, max_vel: i16, max_tqe: i16) -> Result<()> {
        let mut data = [0u8; 8];
        data[0..2].copy_from_slice(&angle.to_le_bytes());
        data[2..4].copy_from_slice(&max_vel.to_le_bytes());
        data[4..6].copy_from_slice(&max_tqe.to_le_bytes());
        data[6] = 0x50;
        data[7] = 0x50;

        self.send_frame(0x0090, &data)
    }

    /// Enable motor for velocity control
    pub fn enable_velocity_mode(&self, motor_id: u8) -> Result<()> {
        let motor_id = motor_id as u32;

        // Set mode to 0x0A (Position Mode)
        let mode_data = [0x01, 0x00, 0x0A, 0x50, 0x50, 0x50, 0x50, 0x50];
        self.send_frame(motor_id, &mode_data)?;
        thread::sleep(Duration::from_millis(50));

        // Set torque limit (register 0x22)
        let torque_data = {
            let mut data = [0x0D, 0x22, 0x00, 0x00, 0x00, 0x00, 0x50, 0x50];
            let torque_limit = 3.0f32;
            data[2..6].copy_from_slice(&torque_limit.to_le_bytes());
            data
        };
        self.send_frame(motor_id, &torque_data)?;
        thread::sleep(Duration::from_millis(20));

        // Set PID parameters for velocity control
        let kp_data = {
            let mut data = [0x0D, 0x23, 0x00, 0x00, 0x00, 0x00, 0x50, 0x50];
            let kp = 2.0f32;
            data[2..6].copy_from_slice(&kp.to_le_bytes());
            data
        };
        self.send_frame(motor_id, &kp_data)?;

        let kd_data = {
            let mut data = [0x0D, 0x24, 0x00, 0x00, 0x00, 0x00, 0x50, 0x50];
            let kd = 0.2f32;
            data[2..6].copy_from_slice(&kd.to_le_bytes());
            data
        };
        self.send_frame(motor_id, &kd_data)?;

        Ok(())
    }

    /// Convert degrees to position integer
    pub fn degrees_to_position(angle_deg: f64) -> i16 {
        let pos = (angle_deg / 360.0) * FACTOR_POS;
        pos.max(-32768.0).min(32767.0) as i16
    }

    /// Convert rad/s to velocity integer
    pub fn rps_to_velocity(velocity_rps: f64) -> i16 {
        let vel = velocity_rps * FACTOR_VEL;
        vel.max(-32768.0).min(32767.0) as i16
    }

    /// Convert rad/s² to acceleration integer
    pub fn rps2_to_acceleration(acceleration_rps2: f64) -> i16 {
        let acc = acceleration_rps2 * FACTOR_ACC;
        acc.max(-32768.0).min(32767.0) as i16
    }

    /// Convert Nm to torque integer
    pub fn nm_to_torque(torque_nm: f64) -> i16 {
        let tqe = torque_nm * FACTOR_TQE;
        tqe.max(-32768.0).min(32767.0) as i16
    }
}

// Public conversion functions for binary compatibility
pub fn degrees_to_position(angle_deg: f64) -> i16 {
    let pos = (angle_deg / 360.0) * FACTOR_POS;
    pos.max(-32768.0).min(32767.0) as i16
}

pub fn rps_to_velocity(velocity_rps: f64) -> i16 {
    let vel = velocity_rps * FACTOR_VEL;
    vel.max(-32768.0).min(32767.0) as i16
}

pub fn rps2_to_acceleration(acceleration_rps2: f64) -> i16 {
    let acc = acceleration_rps2 * FACTOR_ACC;
    acc.max(-32768.0).min(32767.0) as i16
}

pub fn nm_to_torque(torque_nm: f64) -> i16 {
    let tqe = torque_nm * FACTOR_TQE;
    tqe.max(-32768.0).min(32767.0) as i16
}