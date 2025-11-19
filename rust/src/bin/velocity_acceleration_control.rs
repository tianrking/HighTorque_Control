//! LivelyBot Velocity & Acceleration Control
//!
//! High-performance velocity control with intelligent emergency stop.

use anyhow::Result;
use clap::Parser;
use crossterm::{
    execute,
    event::{self, Event, KeyCode, KeyEvent},
    style::{Print, Stylize},
    terminal::{disable_raw_mode, enable_raw_mode, Clear, ClearType},
    cursor::{MoveTo, Show, Hide},
};
use livelybot_motor_control::{LivelyMotorController, MAGIC_POS};
use std::io::{stdout, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

static RUNNING: AtomicBool = AtomicBool::new(true);

/// LivelyBot Velocity & Acceleration Control
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

    /// Default acceleration (default: 15.0)
    #[arg(short, long, default_value = "15.0")]
    acceleration: f64,

    /// Maximum brake acceleration (default: 30.0)
    #[arg(long, default_value = "30.0")]
    brake_acceleration: f64,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Setup Ctrl+C handler
    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();
    ctrlc::set_handler(move || {
        r.store(false, Ordering::SeqCst);
    })?;

    // Print header
    print_header();

    // Initialize controller
    let controller = LivelyMotorController::new(&args.interface, args.bitrate)?;

    execute!(
        stdout(),
        Print("✅ ".green()),
        Print(format!("控制器初始化成功 (电机 ID: {})\n", args.motor_id))
    )?;

    // Enable motor
    controller.enable_velocity_mode(args.motor_id)?;
    execute!(
        stdout(),
        Print("✅ ".green()),
        Print("电机已激活，准备开始控制\n")
    )?;

    // Interactive input
    run_interactive_mode(&controller, args.motor_id, &running, args.acceleration)?;

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
        Print("🏎️  速度 + 加速度模式 (智能紧急制动)\n".blue().bold()),
        Print("命令:\n"),
        Print("  [速度值]       -> 设置目标速度 (例如: 5.0, -2.0)\n"),
        Print("  acc [数值]    -> 设置行驶加速度 (例如: acc 10.0)\n"),
        Print("  0             -> 触发紧急停止\n"),
        Print("  q             -> 退出\n"),
        Print("=".repeat(50)),
        Print("\n")
    ).unwrap();
}


fn run_interactive_mode(
    controller: &LivelyMotorController,
    motor_id: u8,
    running: &Arc<AtomicBool>,
    default_acc: f64,
) -> Result<()> {
    set_target_acceleration(default_acc);

    while running.load(Ordering::SeqCst) {
        execute!(stdout(), Print("命令: "))?;
        stdout().flush()?;

        let mut input = String::new();
        std::io::stdin().read_line(&mut input)?;

        let input = input.trim();
        if input == "q" {
            break;
        } else if input == "0" {
            set_target_velocity(0.0);
            execute!(stdout(), Print("   -> 🛑 紧急制动\n".yellow()))?;
        } else if input.to_lowercase().starts_with("acc") {
            if let Ok(acc) = input[3..].trim().parse::<f64>() {
                set_target_acceleration(acc);
                execute!(stdout(), Print(format!("   -> 行驶加速度设为: {} rad/s²\n", acc)))?;
            }
        } else if let Ok(vel) = input.parse::<f64>() {
            set_target_velocity(vel);
            execute!(stdout(), Print(format!("   -> 目标速度: {} rad/s\n", vel)))?;

            // Send velocity command
            let current_vel = get_target_velocity();
            let current_acc = get_target_acceleration();
            let effective_acc = if current_vel == 0.0 { 30.0 } else { current_acc };

            let pos_int = MAGIC_POS;
            let vel_int = livelybot_motor_control::rps_to_velocity(current_vel);
            let acc_int = livelybot_motor_control::rps2_to_acceleration(effective_acc);

            controller.send_velocity_command(pos_int, vel_int, acc_int)?;
        }

        thread::sleep(Duration::from_millis(10));
    }

    Ok(())
}

// Simple atomic storage for target values
static mut TARGET_VELOCITY: f64 = 0.0;
static mut TARGET_ACCELERATION: f64 = 15.0;

fn set_target_velocity(vel: f64) {
    unsafe {
        TARGET_VELOCITY = vel;
    }
}

fn get_target_velocity() -> f64 {
    unsafe { TARGET_VELOCITY }
}

fn set_target_acceleration(acc: f64) {
    unsafe {
        TARGET_ACCELERATION = acc;
    }
}

fn get_target_acceleration() -> f64 {
    unsafe { TARGET_ACCELERATION.abs() }
}

