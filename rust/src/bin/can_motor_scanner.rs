//! LivelyBot CAN Motor Scanner
//!
//! Scans CAN bus for connected LivelyBot motors and displays their information.

use anyhow::Result;
use clap::Parser;
use crossterm::{
    execute,
    style::{Color, Print, Stylize},
};
use livelybot_motor_control::{LivelyMotorController, MotorInfo};
use std::io::{stdout, Write};
use std::time::Duration;
use std::thread;

/// LivelyBot Motor Scanner
#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// Starting motor ID (default: 1)
    #[arg(short, long, default_value = "1")]
    start_id: u8,

    /// Ending motor ID (default: 14)
    #[arg(short, long, default_value = "14")]
    end_id: u8,

    /// CAN interface (default: can0)
    #[arg(short, long, default_value = "can0")]
    interface: String,

    /// CAN bitrate (default: 1000000)
    #[arg(short, long, default_value = "1000000")]
    bitrate: u32,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Print header
    print_header();

    // Initialize controller
    let controller = LivelyMotorController::new(&args.interface, args.bitrate)?;

    execute!(
        stdout(),
        Print("✅ ".green()),
        Print(format!("扫描器初始化成功 (接口: {}, 波特率: {})\n", args.interface, args.bitrate))
    )?;

    // Scan motors
    let motors = scan_motors(&controller, args.start_id, args.end_id)?;

    // Print summary
    print_summary(&motors)?;

    Ok(())
}

fn print_header() {
    execute!(
        stdout(),
        Print("\n"),
        Print("=".repeat(50).cyan()),
        Print("\n"),
        Print("🚀 LivelyBot 高扭矩电机扫描器\n".blue().bold()),
        Print("开始扫描电机 ID (范围: "),
    ).unwrap();
}

fn scan_motors(controller: &LivelyMotorController, start_id: u8, end_id: u8) -> Result<Vec<MotorInfo>> {
    execute!(
        stdout(),
        Print(format!("{}-{}...", start_id, end_id)),
        Print("\n"),
        Print("超时时间: 50ms/电机\n"),
        Print("按 Ctrl+C 可随时停止\n"),
        Print("=".repeat(50)),
        Print("\n")
    )?;

    let mut motors = Vec::new();

    for motor_id in start_id..=end_id {
        execute!(
            stdout(),
            Print(format!("扫描 ID {:2}... ", motor_id))
        )?;

        stdout().flush()?;

        match controller.ping_motor(motor_id) {
            Ok(mut info) => {
                if info.is_online {
                    execute!(
                        stdout(),
                        Print("✅ ".green()),
                        Print(format!("[响应] 发现电机 ID: {} (CAN ID: 0x{:X})\n",
                                   info.motor_id, info.motor_id))
                    )?;
                } else {
                    execute!(stdout(), Print("无响应\n"))?;
                }
                motors.push(info);
            }
            Err(e) => {
                execute!(
                    stdout(),
                    Print(format!("❌ 错误: {}\n", e))
                )?;
                motors.push(MotorInfo {
                    motor_id,
                    ..Default::default()
                });
            }
        }

        thread::sleep(Duration::from_millis(10));
    }

    Ok(motors)
}

fn print_summary(motors: &[MotorInfo]) -> Result<()> {
    let online_count = motors.iter().filter(|m| m.is_online).count();

    execute!(
        stdout(),
        Print("\n"),
        Print("=".repeat(50)),
        Print("\n"),
        Print(format!("扫描完成！发现 {} 台电机在线\n", online_count))
    )?;

    if online_count > 0 {
        execute!(stdout(), Print("\n在线电机详情:\n"))?;

        for motor in motors {
            if motor.is_online {
                execute!(
                    stdout(),
                    Print("  ID ".cyan()),
                    Print(format!("{}", motor.motor_id)),
                    Print(" - ".cyan()),
                    Print(&motor.name),
                    Print(format!(" (响应时间: {}ms)\n", motor.response_time_ms))
                )?;
            }
        }
    }

    execute!(
        stdout(),
        Print("=".repeat(50)),
        Print("\n")
    )?;

    Ok(())
}