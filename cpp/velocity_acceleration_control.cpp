/**
 * LivelyBot Velocity & Acceleration Control
 * Simple velocity control utility using SocketCAN
 * g++ -std=c++17 -o velocity_acceleration_control velocity_acceleration_control.cpp -pthread
 */

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <algorithm>
#include <cctype>

const double TWO_PI = 6.28318530717;
const int MAGIC_POS = -32768;  // 0x8000 (Int16 Min) -> 代表"无位置限制"
const double FACTOR_VEL = 4000.0;  // 1r/s = 4000
const double FACTOR_ACC = 1000.0;  // 1r/s² = 1000

volatile sig_atomic_t g_running = 1;

void signalHandler(int signum) {
    (void)signum; // 避免未使用参数警告
    g_running = 0;
}

class SimpleVelocityController {
private:
    std::string channel_;
    int bitrate_;
    int motor_id_;
    double default_acc_;
    double max_brake_acc_;
    int socket_fd_;

    std::atomic<double> target_velocity_;
    std::atomic<double> target_acceleration_;
    std::atomic<bool> running_;

public:
    SimpleVelocityController(const std::string& channel, int bitrate, int motor_id,
                           double default_acc, double max_brake_acc)
        : channel_(channel), bitrate_(bitrate), motor_id_(motor_id),
          default_acc_(default_acc), max_brake_acc_(max_brake_acc),
          socket_fd_(-1), target_velocity_(0.0), target_acceleration_(default_acc),
          running_(false) {}

    ~SimpleVelocityController() {
        stopControl();
        disableMotor();
        if (socket_fd_ >= 0) {
            close(socket_fd_);
        }
    }

    bool initialize() {
        std::cout << "初始化 CAN: " << channel_ << std::endl;

        // 创建 socket
        socket_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (socket_fd_ < 0) {
            std::cerr << "❌ 错误: 无法创建 CAN socket" << std::endl;
            return false;
        }

        // 设置 CAN 接口
        struct ifreq ifr;
        strcpy(ifr.ifr_name, channel_.c_str());
        if (ioctl(socket_fd_, SIOCGIFINDEX, &ifr) < 0) {
            std::cerr << "❌ 错误: CAN 接口 " << channel_ << " 不存在" << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        // 绑定 socket 到接口
        struct sockaddr_can addr;
        memset(&addr, 0, sizeof(addr));
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "❌ 错误: 无法绑定到 CAN 接口" << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        if (!enableMotor()) {
            return false;
        }

        std::cout << "✅ 控制器初始化成功" << std::endl;
        return true;
    }

    bool sendFrame(uint32_t arbitration_id, const uint8_t* data, uint8_t data_length) {
        if (socket_fd_ < 0) return false;

        struct can_frame frame;
        frame.can_id = arbitration_id;
        frame.can_dlc = data_length;
        memcpy(frame.data, data, data_length);

        return write(socket_fd_, &frame, sizeof(struct can_frame)) == sizeof(struct can_frame);
    }

    bool enableMotor() {
        std::cout << "-> [ID " << motor_id_ << "] 初始化中 (速度+加速度模式 0xAD)..." << std::endl;

        uint32_t arb_id = motor_id_;

        // 1. 写入模式: 0x0A (位置/控制模式)
        uint8_t data1[8] = {0x01, 0x00, 0x0A, 0x50, 0x50, 0x50, 0x50, 0x50};
        if (!sendFrame(arb_id, data1, 8)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 2. 设置力矩限制 (寄存器 0x22)
        std::cout << "   >>> 设置力矩限制: 3.0 Nm" << std::endl;
        uint8_t data2[8] = {0x0D, 0x22};
        float torque_limit = 3.0f;
        memcpy(&data2[2], &torque_limit, sizeof(torque_limit));
        memset(&data2[6], 0x50, 2);
        if (!sendFrame(arb_id, data2, 8)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // 3. 设置 PID (速度环)
        uint8_t data3[8] = {0x0D, 0x23};
        float kp = 2.0f;
        memcpy(&data3[2], &kp, sizeof(kp));
        memset(&data3[6], 0x50, 2);
        if (!sendFrame(arb_id, data3, 8)) return false;

        uint8_t data4[8] = {0x0D, 0x24};
        float kd = 0.2f;
        memcpy(&data4[2], &kd, sizeof(kd));
        memset(&data4[6], 0x50, 2);
        if (!sendFrame(arb_id, data4, 8)) return false;

        std::cout << "✅ 初始化完成" << std::endl;
        return true;
    }

    bool disableMotor() {
        uint32_t arb_id = motor_id_;
        uint8_t data[8] = {0x01, 0x00, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50};

        bool result = sendFrame(arb_id, data, 8);
        if (result) {
            std::cout << "🛑 电机已禁用" << std::endl;
        }
        return result;
    }

    void send0xADCommand(int16_t position, int16_t velocity, int16_t acceleration) {
        uint8_t data[8];

        // 打包数据: [PosL, PosH, VelL, VelH, AccL, AccH, 0x50, 0x50]
        data[0] = static_cast<uint8_t>(position & 0xFF);
        data[1] = static_cast<uint8_t>((position >> 8) & 0xFF);
        data[2] = static_cast<uint8_t>(velocity & 0xFF);
        data[3] = static_cast<uint8_t>((velocity >> 8) & 0xFF);
        data[4] = static_cast<uint8_t>(acceleration & 0xFF);
        data[5] = static_cast<uint8_t>((acceleration >> 8) & 0xFF);
        data[6] = 0x50;
        data[7] = 0x50;

        sendFrame(0x00AD, data, 8);
    }

    void startControl() {
        if (running_.load()) return;
        running_ = true;
        std::thread(&SimpleVelocityController::controlLoop, this).detach();
    }

    void stopControl() {
        running_ = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void setVelocity(double velocity) {
        target_velocity_.store(velocity);
        if (velocity == 0.0) {
            std::cout << "   -> 🛑 紧急制动 (加速度=" << max_brake_acc_ << ")" << std::endl;
        } else {
            std::cout << "   -> 目标速度: " << velocity << " rad/s" << std::endl;
        }
    }

    void setAcceleration(double acceleration) {
        target_acceleration_.store(std::abs(acceleration));
        std::cout << "   -> 行驶加速度设为: " << target_acceleration_.load() << " rad/s²" << std::endl;
    }

private:
    void controlLoop() {
        auto last_print = std::chrono::steady_clock::now();

        while (running_.load()) {
            double current_vel = target_velocity_.load();
            double current_acc = target_acceleration_.load();

            // 智能制动逻辑
            double effective_acc = current_acc;
            if (current_vel == 0.0) {
                effective_acc = max_brake_acc_;
            }

            // 转换参数
            int16_t pos_int = MAGIC_POS;  // 0x8000 表示速度模式
            int16_t vel_int = static_cast<int16_t>(current_vel * FACTOR_VEL);
            vel_int = static_cast<int16_t>(std::max(-32768.0, std::min(32767.0, static_cast<double>(vel_int))));
            int16_t acc_int = static_cast<int16_t>(effective_acc * FACTOR_ACC);
            acc_int = static_cast<int16_t>(std::max(-32768.0, std::min(32767.0, static_cast<double>(acc_int))));

            // 发送 0xAD 命令
            send0xADCommand(pos_int, vel_int, acc_int);

            // 每100ms打印一次状态
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_print).count() >= 100) {
                printStatus();
                last_print = now;
            }

            // 控制频率: 100Hz (10ms周期)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void printStatus() {
        double vel = target_velocity_.load();
        double acc = target_acceleration_.load();

        std::cout << "\r(速度=" << std::fixed << std::setprecision(1) << vel << ", 加速度="
                  << std::setprecision(1) << acc << ") > " << std::flush;
    }
};

void showHelp() {
    std::cout << "LivelyBot 速度加速度控制" << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  ./velocity_acceleration_control [motor_id]" << std::endl;
    std::cout << "参数:" << std::endl;
    std::cout << "  motor_id  电机ID (默认: 1)" << std::endl;
    std::cout << "交互模式命令:" << std::endl;
    std::cout << "  [速度值]       设置目标速度 (例如: 5.0, -2.0)" << std::endl;
    std::cout << "  acc [数值]     设置行驶加速度 (例如: acc 10.0)" << std::endl;
    std::cout << "  0              触发紧急停止" << std::endl;
    std::cout << "  q              退出" << std::endl;
}

void runInteractiveMode(SimpleVelocityController& controller) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "🏎️  速度 + 加速度模式 (智能紧急制动)" << std::endl;
    std::cout << "命令:" << std::endl;
    std::cout << "  [速度值]       -> 设置目标速度 (例如: 5.0, -2.0)" << std::endl;
    std::cout << "  acc [数值]    -> 设置行驶加速度 (例如: acc 10.0)" << std::endl;
    std::cout << "  0             -> 触发紧急停止" << std::endl;
    std::cout << "  q             -> 退出" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    std::string input;
    while (g_running) {
        std::cout << "命令: " << std::flush;

        // 检查输入流是否有效
        if (!std::cin.good()) {
            break;
        }

        std::getline(std::cin, input);

        if (input == "q" || input == "exit") {
            break;
        } else if (input.empty()) {
            continue;
        }

        std::transform(input.begin(), input.end(), input.begin(), ::tolower);

        if (input.substr(0, 3) == "acc" && input.length() > 4) {
            double value = std::stod(input.substr(4));
            controller.setAcceleration(value);
        } else {
            try {
                double value = std::stod(input);
                controller.setVelocity(value);
            } catch (...) {
                std::cout << "无效输入" << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "LivelyBot High Torque Velocity Control" << std::endl;
    std::cout << "=====================================" << std::endl;

    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        showHelp();
        return 0;
    }

    int motor_id = 1;
    if (argc >= 2) {
        motor_id = std::atoi(argv[1]);
    }

    SimpleVelocityController controller("can0", 1000000, motor_id, 15.0, 30.0);

    if (!controller.initialize()) {
        std::cerr << "控制器初始化失败" << std::endl;
        return 1;
    }

    controller.startControl();

    try {
        runInteractiveMode(controller);
    } catch (const std::exception& e) {
        std::cout << "\n错误: " << e.what() << std::endl;
    }

    controller.setVelocity(0.0);
    controller.stopControl();

    std::cout << "\n程序结束" << std::endl;
    return 0;
}