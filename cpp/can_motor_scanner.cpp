/**
 * LivelyBot Motor Scanner
 * Simple motor scanning utility using SocketCAN
 * g++ -o can_motor_scanner can_motor_scanner.cpp -pthread
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

struct MotorInfo {
    int motor_id;
    bool is_online;
    std::string name;
    std::string hardware_version;
    int response_time_ms;
};

class SimpleMotorScanner {
private:
    std::string channel_;
    int bitrate_;
    int socket_fd_;

public:
    SimpleMotorScanner(const std::string& channel = "can0", int bitrate = 1000000)
        : channel_(channel), bitrate_(bitrate), socket_fd_(-1) {}

    ~SimpleMotorScanner() {
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

        std::cout << "✅ 扫描器初始化成功" << std::endl;
        return true;
    }

    bool sendPing(int motor_id) {
        struct can_frame frame;
        frame.can_id = (0x8000 | motor_id) | CAN_EFF_FLAG;  // 高8位Bit15=1表示需回复，低8位为电机ID + 扩展帧标志
        frame.can_dlc = 8;
        frame.data[0] = 0x11;  // CMD: 0x11 = 读(0x1_) + int8(0x_0) + 1个数据(0x_1)
        frame.data[1] = 0x00;  // 地址: 0x00 = 读取电机模式
        // 其余用0x50填充，符合SDK协议
        memset(&frame.data[2], 0x50, 6);

        return write(socket_fd_, &frame, sizeof(struct can_frame)) == sizeof(struct can_frame);
    }

    MotorInfo scanMotor(int motor_id) {
        MotorInfo info;
        info.motor_id = motor_id;
        info.is_online = false;
        info.name = "Unknown";
        info.hardware_version = "Unknown";
        info.response_time_ms = -1;

        auto start_time = std::chrono::steady_clock::now();

        if (sendPing(motor_id)) {
            // 等待响应 - 与Python版本相同的超时逻辑
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            struct can_frame frame;
            bool received = false;
            auto timeout_start = std::chrono::steady_clock::now();

            while (!received &&
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - timeout_start).count() < 50) {

                // 设置非阻塞读取超时
                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 10000;  // 10ms
                setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

                ssize_t nbytes = read(socket_fd_, &frame, sizeof(struct can_frame));
                if (nbytes > 0) {
                    uint32_t can_id = frame.can_id & ~CAN_EFF_FLAG;  // 移除扩展帧标志
                    uint32_t raw_id = can_id & 0xFFFF;

                    // 与Python版本完全相同的解析逻辑
                    int source_id = (raw_id >> 8) & 0x7F;
                    int direct_id = can_id & 0xFF;

                    int detected_id = -1;
                    if (source_id > 0 && source_id < 128) {
                        detected_id = source_id;
                    } else if (direct_id == motor_id) {
                        detected_id = direct_id;
                    }

                    if (detected_id == motor_id) {
                        auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start_time).count();

                        info.is_online = true;
                        info.response_time_ms = response_time;

                        std::cout << "✅ [响应] 发现电机 ID: " << detected_id
                                  << " (CAN ID: 0x" << std::hex << can_id << std::dec << ")" << std::endl;

                        // 解析电机信息 - 如果响应包含电机信息
                        if (frame.can_dlc >= 4 && frame.data[0] == 0x51) {
                            // 如果是响应数据帧，解析电机信息
                            char name[5] = {0};
                            memcpy(name, &frame.data[1], 3);
                            info.name = std::string(name);
                        }

                        if (frame.can_dlc >= 8) {
                            char version[5] = {0};
                            memcpy(version, &frame.data[4], 4);
                            info.hardware_version = std::string(version);
                        }

                        received = true;
                    }
                }
            }
        }

        return info;
    }

    std::vector<MotorInfo> scanRange(int start_id, int end_id) {
        std::vector<MotorInfo> motors;

        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "开始扫描电机 ID (范围: " << start_id << "-" << end_id << ")..." << std::endl;
        std::cout << "超时时间: 50ms/电机" << std::endl;
        std::cout << "按 Ctrl+C 可随时停止" << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        for (int motor_id = start_id; motor_id <= end_id; motor_id++) {
            std::cout << "扫描 ID " << std::setw(2) << motor_id << "... " << std::flush;

            auto info = scanMotor(motor_id);
            motors.push_back(info);

            if (!info.is_online) {
                std::cout << "无响应" << std::endl;
            }

            // 与Python版本相同的间隔时间
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return motors;
    }

    void printSummary(const std::vector<MotorInfo>& motors) {
        int online_count = 0;
        for (const auto& motor : motors) {
            if (motor.is_online) online_count++;
        }

        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "扫描完成！发现 " << online_count << " 台电机在线" << std::endl;
        if (online_count > 0) {
            std::cout << "\n在线电机详情:" << std::endl;
            for (const auto& motor : motors) {
                if (motor.is_online) {
                    std::cout << "  ID " << motor.motor_id
                              << " - " << motor.name
                              << " (响应时间: " << motor.response_time_ms << "ms)" << std::endl;
                }
            }
        }
        std::cout << std::string(50, '=') << std::endl;
    }
};

void showHelp() {
    std::cout << "LivelyBot 电机扫描器" << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  ./can_motor_scanner [start_id] [end_id]" << std::endl;
    std::cout << "参数:" << std::endl;
    std::cout << "  start_id  起始电机ID (默认: 1)" << std::endl;
    std::cout << "  end_id    结束电机ID (默认: 14)" << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  ./can_motor_scanner          # 扫描电机1-14" << std::endl;
    std::cout << "  ./can_motor_scanner 1 5      # 扫描电机1-5" << std::endl;
    std::cout << "使用前请确保CAN接口已启用:" << std::endl;
    std::cout << "  sudo ip link set can0 up type can bitrate 1000000" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "LivelyBot High Torque Motor Scanner" << std::endl;
    std::cout << "===================================" << std::endl;

    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        showHelp();
        return 0;
    }

    int start_id = 1;
    int end_id = 14;

    if (argc >= 2) start_id = std::atoi(argv[1]);
    if (argc >= 3) end_id = std::atoi(argv[2]);

    SimpleMotorScanner scanner("can0", 1000000);

    if (!scanner.initialize()) {
        std::cerr << "扫描器初始化失败" << std::endl;
        return 1;
    }

    auto motors = scanner.scanRange(start_id, end_id);
    scanner.printSummary(motors);

    std::cout << "\n扫描完成！" << std::endl;
    return 0;
}