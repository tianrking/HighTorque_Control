/**
 * LivelyBot Angle Stream Control
 * Simple angle control utility using SocketCAN
 * g++ -std=c++17 -o angle_stream_control angle_stream_control.cpp -pthread
 */

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <signal.h>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <cctype>

volatile sig_atomic_t g_running = 1;

void signalHandler(int signum) {
    (void)signum; // 避免未使用参数警告
    g_running = 0;
}

class SimpleAngleController {
private:
    std::string channel_;
    int bitrate_;
    int motor_id_;

    // SDK 系数定义
    static constexpr double FACTOR_POS = 10000.0;  // 1圈 = 10000
    static constexpr double FACTOR_VEL = 4000.0;   // 1r/s = 4000
    static constexpr double FACTOR_TQE = 200.0;    // 通用电机系数

    int socket_fd_;

public:
    SimpleAngleController(const std::string& channel, int bitrate, int motor_id)
        : channel_(channel), bitrate_(bitrate), motor_id_(motor_id), socket_fd_(-1) {}

    ~SimpleAngleController() {
        cleanup();
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

        std::cout << "✅ CAN接口连接成功" << std::endl;
        return true;
    }

    void cleanup() {
        disableMotor();
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
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
        std::cout << "-> [ID " << motor_id_ << "] 发送使能指令 (Register Mode)..." << std::endl;

        uint32_t arb_id = motor_id_;

        // 1. 写入模式: 0x0A (Position Mode)
        uint8_t data1[8] = {0x01, 0x00, 0x0A, 0x50, 0x50, 0x50, 0x50, 0x50};
        sendFrame(arb_id, data1, 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 2. 预设 PID (给一点刚度)
        float kp = 1.0f;
        uint8_t data2[8] = {0x0D, 0x23};
        memcpy(&data2[2], &kp, sizeof(kp));
        memset(&data2[6], 0x50, 2);
        sendFrame(arb_id, data2, 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        float kd = 0.1f;
        uint8_t data3[8] = {0x0D, 0x24};
        memcpy(&data3[2], &kd, sizeof(kd));
        memset(&data3[6], 0x50, 2);
        sendFrame(arb_id, data3, 8);

        std::cout << "✅ 电机已激活，准备发送流控制指令" << std::endl;
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

    void send0x90Command(double angle_deg, double max_vel_rps, double max_tqe_nm) {
        // 1. 计算数值
        int16_t pos_int = static_cast<int16_t>((angle_deg / 360.0) * FACTOR_POS);
        int16_t vel_int = static_cast<int16_t>(max_vel_rps * FACTOR_VEL);
        int16_t tqe_int = static_cast<int16_t>(max_tqe_nm * FACTOR_TQE);

        // 限幅 Int16
        pos_int = static_cast<int16_t>(std::max(-32768.0, std::min(32767.0, static_cast<double>(pos_int))));
        vel_int = static_cast<int16_t>(std::max(-32768.0, std::min(32767.0, static_cast<double>(vel_int))));
        tqe_int = static_cast<int16_t>(std::max(-32768.0, std::min(32767.0, static_cast<double>(tqe_int))));

        // 2. 打包数据: [PosL, PosH, VelL, VelH, TqeL, TqeH, 0x50, 0x50]
        uint8_t data[8];
        data[0] = static_cast<uint8_t>(pos_int & 0xFF);
        data[1] = static_cast<uint8_t>((pos_int >> 8) & 0xFF);
        data[2] = static_cast<uint8_t>(vel_int & 0xFF);
        data[3] = static_cast<uint8_t>((vel_int >> 8) & 0xFF);
        data[4] = static_cast<uint8_t>(tqe_int & 0xFF);
        data[5] = static_cast<uint8_t>((tqe_int >> 8) & 0xFF);
        data[6] = 0x50;
        data[7] = 0x50;

        sendFrame(0x0090, data, 8);

        std::cout << "   >>> 0x90流指令: Ang=" << angle_deg << "° Vel=" << max_vel_rps
                  << " Tqe=" << max_tqe_nm << " (原始值: [" << pos_int << "," << vel_int
                  << "," << tqe_int << "])" << std::endl;
    }

    void setAngle(double angle_deg, double max_vel_rps = 2.0, double max_tqe_nm = 3.0, int send_count = 5) {
        for (int i = 0; i < send_count; ++i) {
            send0x90Command(angle_deg, max_vel_rps, max_tqe_nm);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "   -> 目标角度: " << angle_deg << " 度" << std::endl;
    }

    void runInteractiveControl() {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "🚀 0x90 流控制模式 (复刻 SDK)" << std::endl;
        std::cout << "输入角度 (如 90) 回车。" << std::endl;
        std::cout << "默认参数: 限速 2.0 r/s, 限矩 3.0 Nm" << std::endl;
        std::cout << "输入 q 退出" << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        try {
            // 先使能电机
            enableMotor();

            while (g_running) {
                std::cout << "(Stream 0x90) > " << std::flush;

                // 检查输入流是否有效
                if (!std::cin.good()) {
                    break;
                }

                std::string input;
                std::getline(std::cin, input);

                if (input == "q" || input == "exit") {
                    break;
                }
                if (input.empty()) {
                    continue;
                }

                try {
                    double deg = std::stod(input);
                    setAngle(deg);
                } catch (...) {
                    std::cout << "输入错误" << std::endl;
                }
            }
        } catch (...) {
            std::cout << "\n中断" << std::endl;
        }
    }

    void runSineWave(double amplitude_deg, double frequency_hz, double duration_sec) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "🌊 正弦波角度控制" << std::endl;
        std::cout << "幅值: " << amplitude_deg << "°, 频率: " << frequency_hz
                  << " Hz, 时长: " << duration_sec << "s" << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        try {
            enableMotor();

            auto start_time = std::chrono::steady_clock::now();
            while (g_running && std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - start_time).count() < duration_sec) {

                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start_time).count() / 1000.0;
                double target_deg = amplitude_deg * std::sin(2.0 * M_PI * frequency_hz * elapsed);

                setAngle(target_deg);

                std::cout << "\r目标: " << std::fixed << std::setprecision(1) << target_deg << "°" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

        } catch (...) {
            std::cout << "\n中断" << std::endl;
        }
    }

    void runStepControl(const std::vector<double>& angles, double step_duration_sec) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "📈 阶梯角度控制" << std::endl;
        std::cout << "角度序列: ";
        for (size_t i = 0; i < angles.size(); ++i) {
            std::cout << angles[i];
            if (i < angles.size() - 1) std::cout << ", ";
        }
        std::cout << "°" << std::endl;
        std::cout << "每步时长: " << step_duration_sec << "s" << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        try {
            enableMotor();

            for (size_t i = 0; i < angles.size() && g_running; ++i) {
                std::cout << "\n--- 步骤 " << (i + 1) << "/" << angles.size() << ": "
                          << angles[i] << "° ---" << std::endl;

                setAngle(angles[i]);

                auto step_start = std::chrono::steady_clock::now();
                while (g_running && std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::steady_clock::now() - step_start).count() < step_duration_sec) {

                    auto remaining = step_duration_sec -
                                   std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::steady_clock::now() - step_start).count();
                    std::cout << "\r剩余时间: " << std::fixed << std::setprecision(1)
                              << remaining << "s" << std::flush;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }

        } catch (...) {
            std::cout << "\n中断" << std::endl;
        }
    }

    void testPositions(const std::vector<double>& positions) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "🧪 多位置测试" << std::endl;
        std::cout << "测试位置: ";
        for (size_t i = 0; i < positions.size(); ++i) {
            std::cout << positions[i];
            if (i < positions.size() - 1) std::cout << ", ";
        }
        std::cout << "°" << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        try {
            enableMotor();

            for (size_t i = 0; i < positions.size() && g_running; ++i) {
                std::cout << "\n--- 测试位置 " << (i + 1) << "/" << positions.size()
                          << ": " << positions[i] << "° ---" << std::endl;

                setAngle(positions[i]);

                std::cout << "等待2秒稳定..." << std::flush;
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }

        } catch (...) {
            std::cout << "\n中断" << std::endl;
        }
    }
};

void showHelp() {
    std::cout << "LivelyBot 角度流控制" << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  ./angle_stream_control [motor_id] [mode] [options]" << std::endl;
    std::cout << "参数:" << std::endl;
    std::cout << "  motor_id  电机ID (默认: 1)" << std::endl;
    std::cout << "  mode      控制模式 (interactive, sine, step, test)" << std::endl;
    std::cout << "模式选项:" << std::endl;
    std::cout << "  sine:     --amplitude <度数> --frequency <Hz> --duration <秒>" << std::endl;
    std::cout << "  step:     --angles <角度列表,逗号分隔> --step-time <秒>" << std::endl;
    std::cout << "  test:     --positions <位置列表,逗号分隔>" << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  ./angle_stream_control 1 interactive" << std::endl;
    std::cout << "  ./angle_stream_control 1 sine --amplitude 90 --frequency 0.2 --duration 10" << std::endl;
    std::cout << "  ./angle_stream_control 1 step --angles \"0,45,90,45,0\" --step-time 3" << std::endl;
}

std::vector<double> parseDoubleList(const std::string& str) {
    std::vector<double> result;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        result.push_back(std::stod(token));
    }
    return result;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "LivelyBot High Torque Angle Control" << std::endl;
    std::cout << "===================================" << std::endl;

    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        showHelp();
        return 0;
    }

    int motor_id = 1;
    std::string mode = "interactive";
    double amplitude = 90.0;
    double frequency = 0.2;
    double duration = 10.0;
    std::vector<double> angles = {0.0, 45.0, 90.0, 45.0, 0.0};
    double step_time = 3.0;
    std::vector<double> positions = {0.0, 30.0, 60.0, 90.0, 60.0, 30.0, 0.0};

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--motor-id" && i + 1 < argc) {
            motor_id = std::atoi(argv[++i]);
        } else if (arg == "--amplitude" && i + 1 < argc) {
            amplitude = std::stod(argv[++i]);
        } else if (arg == "--frequency" && i + 1 < argc) {
            frequency = std::stod(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration = std::stod(argv[++i]);
        } else if (arg == "--angles" && i + 1 < argc) {
            angles = parseDoubleList(argv[++i]);
        } else if (arg == "--step-time" && i + 1 < argc) {
            step_time = std::stod(argv[++i]);
        } else if (arg == "--positions" && i + 1 < argc) {
            positions = parseDoubleList(argv[++i]);
        } else if (arg.substr(0, 2) != "--") {
            if (i == 1) motor_id = std::atoi(arg.c_str());
            else if (i == 2) mode = arg;
        }
    }

    SimpleAngleController controller("can0", 1000000, motor_id);

    if (!controller.initialize()) {
        std::cerr << "控制器初始化失败" << std::endl;
        return 1;
    }

    try {
        if (mode == "interactive") {
            controller.runInteractiveControl();
        } else if (mode == "sine") {
            controller.runSineWave(amplitude, frequency, duration);
        } else if (mode == "step") {
            controller.runStepControl(angles, step_time);
        } else if (mode == "test") {
            controller.testPositions(positions);
        } else {
            std::cerr << "未知模式: " << mode << std::endl;
            showHelp();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "\n❌ 控制过程中出错: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n程序结束" << std::endl;
    return 0;
}