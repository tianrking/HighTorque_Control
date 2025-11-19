//! LivelyBot Angle Stream Control
//!
//! High-performance angle control with MIT-style impedance control.

use anyhow::Result;
use clap::{Parser, Subcommand};
use crossterm::{
    execute,
    style::{Print, Stylize},
    terminal::Clear,
    terminal::ClearType,
    cursor::MoveTo,
};
use livelybot_motor_control::{LivelyMotorController};
use std::f64::consts::PI;
use std::io::{stdout, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

static RUNNING: AtomicBool = AtomicBool::new(true);

/// LivelyBot Angle Stream Control
#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// Motor ID (default: 1)
    #[arg(short, long, default_value = "1")]
    motor_id: u8,

    /// CAN interface (default: can0)
    #[arg(short, long, default_value = "can0")]
    interface: String,

    /// CAN bitrate (default: 1000000)
    #[arg(short, long, default_value = "1000000")]
    bitrate: u32,

    #[command(subcommand)]
    mode: Option<Mode>,
}

#[derive(Subcommand)]
enum Mode {
    /// Interactive angle control
    Interactive,
    /// Sine wave control
    Sine {
        /// Amplitude in degrees
        #[arg(long, default_value = "90.0")]
        amplitude: f64,
        /// Frequency in Hz
        #[arg(long, default_value = "0.2")]
        frequency: f64,
        /// Duration in seconds
        #[arg(long, default_value = "10.0")]
        duration: f64,
    },
    /// Step control
    Step {
        /// Comma-separated angles
        #[arg(long)]
        angles: String,
        /// Time per step in seconds
        #[arg(long, default_value = "3.0")]
        step_time: f64,
    },
    /// Multi-position test
    Test {
        /// Comma-separated positions
        #[arg(long, default_value = "0,30,60,90,60,30,0")]
        positions: String,
    },
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Setup Ctrl+C handler
    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();
    ctrlc::set_handler(move || {
        r.store(false, Ordering::SeqCst);
    })?;

    // Initialize controller
    let controller = LivelyMotorController::new(&args.interface, args.bitrate)?;

    execute!(
        stdout(),
        Print("✅ ".green()),
        Print(format!("控制器初始化成功 (电机 ID: {})\n", args.motor_id))
    )?;

    // Enable motor
    controller.enable_motor(args.motor_id)?;
    execute!(
        stdout(),
        Print("✅ ".green()),
        Print("电机已激活，准备发送流控制指令\n")
    )?;

    // Run the specified mode
    let mode = args.mode.unwrap_or(Mode::Interactive);
    match mode {
        Mode::Interactive => run_interactive_mode(&controller, args.motor_id, &running)?,
        Mode::Sine { amplitude, frequency, duration } => {
            run_sine_wave(&controller, args.motor_id, &running, amplitude, frequency, duration)?
        }
        Mode::Step { angles, step_time } => {
            let angle_list = parse_double_list(&angles)?;
            run_step_control(&controller, args.motor_id, &running, &angle_list, step_time)?
        }
        Mode::Test { positions } => {
            let position_list = parse_double_list(&positions)?;
            test_positions(&controller, args.motor_id, &running, &position_list)?
        }
    }

    // Cleanup
    controller.disable_motor(args.motor_id)?;
    execute!(stdout(), Print("🛑 ".yellow()), Print("电机已禁用\n"))?;

    Ok(())
}

fn print_header() {
    execute!(
        stdout(),
        Print("\n"),
        Print("=".repeat(50).cyan()),
        Print("\n"),
        Print("🚀 0x90 流控制模式 (复刻 SDK)\n".blue().bold()),
        Print("输入角度 (如 90) 回车。\n"),
        Print("默认参数: 限速 2.0 r/s, 限矩 3.0 Nm\n"),
        Print("输入 q 退出\n"),
        Print("=".repeat(50)),
        Print("\n")
    ).unwrap();
}

fn run_interactive_mode(
    controller: &LivelyMotorController,
    motor_id: u8,
    running: &Arc<AtomicBool>,
) -> Result<()> {
    print_header();

    while running.load(Ordering::SeqCst) {
        execute!(stdout(), Print("(Stream 0x90) > "))?;
        stdout().flush()?;

        let mut input = String::new();
        std::io::stdin().read_line(&mut input)?;

        let input = input.trim();
        if input.to_lowercase() == "q" {
            break;
        } else if let Ok(angle) = input.parse::<f64>() {
            set_angle(controller, motor_id, angle, 2.0, 3.0, 5)?;
            execute!(
                stdout(),
                Print(format!("   -> 目标角度: {} 度\n", angle))
            )?;
        } else if !input.is_empty() {
            execute!(stdout(), Print("输入错误\n".red()))?;
        }
    }

    execute!(stdout(), Print("\n"))?;
    Ok(())
}

fn run_sine_wave(
    controller: &LivelyMotorController,
    motor_id: u8,
    running: &Arc<AtomicBool>,
    amplitude_deg: f64,
    frequency_hz: f64,
    duration_sec: f64,
) -> Result<()> {
    execute!(
        stdout(),
        Print("\n"),
        Print("=".repeat(50)),
        Print("\n"),
        Print("🌊 正弦波角度控制\n".blue()),
        Print(format!("幅值: {}°, 频率: {} Hz, 时长: {}s\n", amplitude_deg, frequency_hz, duration_sec)),
        Print("=".repeat(50)),
        Print("\n")
    )?;

    let start_time = Instant::now();

    while running.load(Ordering::SeqCst) && start_time.elapsed().as_secs_f64() < duration_sec {
        let elapsed = start_time.elapsed().as_secs_f64();
        let target_deg = amplitude_deg * (2.0 * PI * frequency_hz * elapsed).sin();

        set_angle(controller, motor_id, target_deg, 2.0, 3.0, 5)?;

        execute!(
            stdout(),
            MoveTo(0, 15),
            Clear(ClearType::CurrentLine),
            Print(format!("目标: {:.1}°", target_deg))
        )?;

        stdout().flush()?;
        thread::sleep(Duration::from_millis(10));
    }

    Ok(())
}

fn run_step_control(
    controller: &LivelyMotorController,
    motor_id: u8,
    running: &Arc<AtomicBool>,
    angles: &[f64],
    step_duration_sec: f64,
) -> Result<()> {
    execute!(
        stdout(),
        Print("\n"),
        Print("=".repeat(50)),
        Print("\n"),
        Print("📈 阶梯角度控制\n".blue()),
        Print("角度序列: "),
    )?;

    for (i, &angle) in angles.iter().enumerate() {
        if i > 0 {
            execute!(stdout(), Print(", "))?;
        }
        execute!(stdout(), Print(angle))?;
    }
    execute!(
        stdout(),
        Print("°\n"),
        Print(format!("每步时长: {}s\n", step_duration_sec)),
        Print("=".repeat(50)),
        Print("\n")
    )?;

    for (step, &angle) in angles.iter().enumerate() {
        if !running.load(Ordering::SeqCst) {
            break;
        }

        execute!(
            stdout(),
            Print(format!("\n--- 步骤 {}/{}: {}° ---\n", step + 1, angles.len(), angle))
        )?;

        set_angle(controller, motor_id, angle, 2.0, 3.0, 5)?;

        let step_start = Instant::now();
        while running.load(Ordering::SeqCst) && step_start.elapsed().as_secs_f64() < step_duration_sec {
            let remaining = step_duration_sec - step_start.elapsed().as_secs_f64();
            execute!(
                stdout(),
                MoveTo(0, 20),
                Clear(ClearType::CurrentLine),
                Print(format!("剩余时间: {:.1}s", remaining))
            )?;

            stdout().flush()?;
            thread::sleep(Duration::from_millis(100));
        }
    }

    Ok(())
}

fn test_positions(
    controller: &LivelyMotorController,
    motor_id: u8,
    running: &Arc<AtomicBool>,
    positions: &[f64],
) -> Result<()> {
    execute!(
        stdout(),
        Print("\n"),
        Print("=".repeat(50)),
        Print("\n"),
        Print("🧪 多位置测试\n".blue()),
        Print("测试位置: "),
    )?;

    for (i, &pos) in positions.iter().enumerate() {
        if i > 0 {
            execute!(stdout(), Print(", "))?;
        }
        execute!(stdout(), Print(pos))?;
    }
    execute!(
        stdout(),
        Print("°\n"),
        Print("=".repeat(50)),
        Print("\n")
    )?;

    for (i, &position) in positions.iter().enumerate() {
        if !running.load(Ordering::SeqCst) {
            break;
        }

        execute!(
            stdout(),
            Print(format!("\n--- 测试位置 {}/{}: {}° ---\n", i + 1, positions.len(), position))
        )?;

        set_angle(controller, motor_id, position, 2.0, 3.0, 5)?;

        execute!(stdout(), Print("等待2秒稳定..."))?;
        stdout().flush()?;
        thread::sleep(Duration::from_secs(2));
    }

    Ok(())
}

fn set_angle(
    controller: &LivelyMotorController,
    motor_id: u8,
    angle_deg: f64,
    max_vel_rps: f64,
    max_tqe_nm: f64,
    send_count: usize,
) -> Result<()> {
    let pos_int = livelybot_motor_control::degrees_to_position(angle_deg);
    let vel_int = livelybot_motor_control::rps_to_velocity(max_vel_rps);
    let tqe_int = livelybot_motor_control::nm_to_torque(max_tqe_nm);

    for _ in 0..send_count {
        controller.send_angle_command(pos_int, vel_int, tqe_int)?;
        thread::sleep(Duration::from_millis(10));
    }

    Ok(())
}

fn parse_double_list(s: &str) -> Result<Vec<f64>> {
    s.split(',')
        .map(|s| s.trim().parse::<f64>().map_err(Into::into))
        .collect()
}