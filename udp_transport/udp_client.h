#pragma once
#include <netinet/in.h>            /** 网络通信相关头文件 */
#include <string.h>                /** 字符串处理函数 */
#include <sys/socket.h>            /** 套接字编程接口 */
#include <sys/types.h>             /** 数据类型定义 */
#include <unistd.h>                /** 提供 POSIX 操作系统 API 的访问，如 close() */
#include <memory>                  /** 提供智能指针等功能 */
#include <string>                  /** C++ 标准库字符串类 */
#include <vector>                  /** C++ 标准库动态数组容器 */
#include "data_segment.h"          /** 自定义数据段类，用于 UDP 传输 */

namespace safe_udp
{
    /** 客户端默认文件存储路径 */
    constexpr char CLIENT_FILE_PATH[] = "/work/files/client_files/";

    /**
     * UdpClient 类用于实现客户端的 UDP 通信逻辑。
     * 主要功能包括：
     * - 创建 UDP socket
     * - 向服务器请求文件
     * - 接收文件数据并缓存
     * - 发送 ACK 确认信息
     * - 处理丢包和延迟模拟
     */
    class UdpClient
    {
    public:
        /**
         * 构造函数：初始化客户端参数
         */
        UdpClient();

        /**
         * 析构函数：关闭 socket
         */
        ~UdpClient() { close(sockfd_); }

        /**
         * 向服务器发送文件请求
         *
         * @param file_name 请求的文件名
         */
        void SendFileRequest(const std::string& file_name);

        /**
         * 创建 socket 并连接到指定的服务器地址和端口
         *
         * @param server_address 服务器 IP 地址
         * @param port 服务器端口号
         */
        void CreateSocketAndServerConnection(const std::string& server_address,
                                             const std::string& port);

        int initial_seq_number_; /** 初始序列号 */
        bool is_packet_drop_; /** 是否启用丢包模拟 */
        bool is_delay_; /** 是否启用延迟模拟 */
        int prob_value_; /** 丢包或延迟的概率值 */

        int last_in_order_packet_; /** 最后一个按序到达的数据包编号 */
        int last_packet_received_; /** 最后收到的数据包编号 */
        int receiver_window_; /** 接收窗口大小 */
        bool fin_flag_received_; /** 是否已收到 FIN 标志 */

    private:
        /**
         * 发送 ACK 确认信息给服务器
         *
         * @param ackNumber 要确认的序列号
         */
        void send_ack(int ackNumber);

        /**
         * 将数据包插入到数据段向量中
         *
         * @param index 插入位置索引
         * @param data_segment 待插入的数据段
         */
        void insert(int index, const DataSegment& data_segment);

        /**
         * 将数据段添加到数据段向量中
         *
         * @param data_segment 待添加的数据段
         * @return 返回插入的位置索引
         */
        int add_to_data_segment_vector(const DataSegment& data_segment);

        int sockfd_; /** socket 文件描述符 */
        int seq_number_; /** 当前使用的序列号 */
        int ack_number_; /** 当前使用的确认号 */
        int16_t length_; /** 数据长度 */
        struct sockaddr_in server_address_; /** 服务器地址结构体 */
        std::vector<DataSegment> data_segments_; /** 存储接收的数据段 */
    };
} // namespace safe_udp