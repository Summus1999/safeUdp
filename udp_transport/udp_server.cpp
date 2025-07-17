#include "udp_server.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>
#include <glog/logging.h>

namespace safe_udp
{
    UdpServer::UdpServer()
    {
        sliding_window_ = std::make_unique<SlidingWindow>();
        packet_statistics_ = std::make_unique<PacketStatistics>(); /** 创建数据包统计模块，用于记录发送和重传情况 */

        sockfd_ = 0; /** 初始化 socket 文件描述符为 0 */
        smoothed_rtt_ = 20000; /** 平滑往返时间（Smoothed RTT）初始值设为 20000 微秒 */
        smoothed_timeout_ = 30000; /** 初始超时时间设置为 30000 微秒 */
        dev_rtt_ = 0; /** RTT 偏差初始化为 0 */

        initial_seq_number_ = 67; /** 设置初始序列号为 67 */
        start_byte_ = 0; /** 当前传输起始字节位置初始化为 0 */

        ssthresh_ = 128; /** 慢启动阈值初始化为 128 */
        cwnd_ = 1; /** 拥塞窗口初始大小为 1 */

        is_slow_start_ = true; /** 标记当前处于慢启动阶段 */
        is_cong_avd_ = false; /** 标记当前不在拥塞避免阶段 */
        is_fast_recovery_ = false; /** 标记当前不在快速恢复阶段 */
    }

    int UdpServer::StartServer(int port)
    {
        int sfd; /** 定义 socket 文件描述符 */
        struct sockaddr_in server_addr; /** 定义服务器地址结构体 */

        LOG(INFO) << "Starting the webserver... port: " << port;

        /** 创建 UDP socket，AF_INET 表示 IPv4，SOCK_DGRAM 表示无连接的数据报套接字 */
        sfd = socket(AF_INET, SOCK_DGRAM, 0);

        /** 检查 socket 是否创建成功 */
        if (sfd < 0)
        {
            LOG(ERROR) << " Failed to socket !!!";
            exit(0); /** 创建失败，退出程序 */
        }

        /** 清空 server_addr 结构体，确保初始化为 0 */
        memset(&server_addr, 0, sizeof(server_addr));

        /** 设置服务器地址族为 AF_INET（IPv4） */
        server_addr.sin_family = AF_INET;

        /** 设置服务器监听地址为本地回环地址 127.0.0.1 */
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        /** 设置服务器监听端口号，并将主机字节序转换为网络字节序 */
        server_addr.sin_port = htons(port);

        /** 将 socket 绑定到指定的地址和端口 */
        if (bind(sfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        {
            LOG(ERROR) << "binding error !!!"; /** 绑定失败，记录错误信息 */
            exit(0); /** 退出程序 */
        }

        /** 日志输出绑定成功的地址信息 */
        LOG(INFO) << "**Server Bind set to addr: " << server_addr.sin_addr.s_addr;

        /** 日志输出绑定的端口号 */
        LOG(INFO) << "**Server Bind set to port: " << server_addr.sin_port;

        /** 日志输出地址族信息 */
        LOG(INFO) << "**Server Bind set to family: " << server_addr.sin_family;

        /** 日志输出服务器启动成功信息 */
        LOG(INFO) << "Started successfully";

        sockfd_ = sfd; /** 保存 socket 文件描述符供后续使用 */

        return sfd; /** 返回 socket 文件描述符 */
    }

    /**
     * 打开指定的文件以进行传输。
     *
     * @param file_name 要打开的文件名
     * @return 如果文件成功打开则返回 true，否则返回 false
     */
    bool UdpServer::OpenFile(const std::string& file_name)
    {
        LOG(INFO) << "Opening the file " << file_name;

        /** 使用输入模式打开文件 */
        file_.open(file_name.c_str(), std::ios::in);

        /** 检查文件是否成功打开 */
        if (!this->file_.is_open())
        {
            LOG(INFO) << "File: " << file_name << " opening failed";
            return false; /** 文件打开失败，返回 false */
        }
        else
        {
            LOG(INFO) << "File: " << file_name << " opening success";
            return true; /** 文件打开成功，返回 true */
        }
    }

    /** 文件传输函数 */
    void UdpServer::StartFileTransfer()
    {
        LOG(INFO) << "Starting the file_ transfer ";

        /** 将文件指针移动到文件末尾以获取文件大小 */
        file_.seekg(0, std::ios::end);
        file_length_ = file_.tellg(); /** 获取文件总长度 */

        /** 将文件指针重置到文件开头 */
        file_.seekg(0, std::ios::beg);

        /** 开始发送文件数据 */
        send();
    }

    /**
     * 向客户端发送错误信息（文件未找到）。
     */
    void UdpServer::SendError()
    {
        std::string error("FILE NOT FOUND");

        /** 发送错误消息到客户端 */
        sendto(sockfd_, error.c_str(), error.size(), 0,
               (struct sockaddr*)&cli_address_, sizeof(cli_address_));
    }

    /**
     * 发送函数，用于通过 UDP 发送文件数据，支持滑动窗口机制和拥塞控制。
     */
    void UdpServer::send()
    {
        LOG(INFO) << "Entering Send()";

        int sent_count = 1;
        int sent_count_limit = 1;

        struct timeval process_start_time;
        struct timeval process_end_time;
        gettimeofday(&process_start_time, NULL); /** 记录发送过程开始时间 */

        /** 如果是第一次发送，重置起始字节位置为 0 */
        if (sliding_window_->last_packet_sent_ == -1)
        {
            start_byte_ = 0;
        }

        /** 循环发送数据直到所有字节都被传输 */
        while (start_byte_ <= file_length_)
        {
            fd_set rfds; /** 文件描述符集合，用于 select */
            struct timeval tv; /** 超时时间结构体 */
            int res; /** select 返回结果 */

            sent_count = 1;
            sent_count_limit = std::min(rwnd_, cwnd_); /** 窗口允许的最大发送数 */

            LOG(INFO) << "SEND START  !!!!";
            LOG(INFO) << "Before the window rwnd_: " << rwnd_ << " cwnd_: " << cwnd_
                << " window used: "
                << sliding_window_->last_packet_sent_ -
                sliding_window_->last_acked_packet_;

            /**
             * 发送数据包，直到窗口已满或没有更多数据可发送
             */
            while (sliding_window_->last_packet_sent_ -
                sliding_window_->last_acked_packet_ <=
                std::min(rwnd_, cwnd_) &&
                sent_count <= sent_count_limit)
            {
                send_packet(start_byte_ + initial_seq_number_, start_byte_);

                /** 统计慢启动阶段发送的数据包数量 */
                if (is_slow_start_)
                {
                    packet_statistics_->slow_start_packet_sent_count_++;
                }
                else if (is_cong_avd_)
                {
                    /** 统计拥塞避免阶段发送的数据包数量 */
                    packet_statistics_->cong_avd_packet_sent_count_++;
                }

                /** 更新下一个要发送的起始字节位置 */
                start_byte_ = start_byte_ + MAX_DATA_SIZE;
                if (start_byte_ > file_length_)
                {
                    LOG(INFO) << "No more data left to be sent";
                    break;
                }
                sent_count++;
            }

            LOG(INFO) << "SEND END !!!!!";

            /** 设置 select 超时时间 */
            FD_ZERO(&rfds);
            FD_SET(sockfd_, &rfds);

            tv.tv_sec = (int64_t)smoothed_timeout_ / 1000000;
            tv.tv_usec = smoothed_timeout_ - tv.tv_sec;

            LOG(INFO) << "SELECT START:" << smoothed_timeout_ << "!!!";
            while (true)
            {
                /** 等待 ACK 或超时 */
                res = select(sockfd_ + 1, &rfds, NULL, NULL, &tv);
                if (res == -1)
                {
                    LOG(ERROR) << "Error in select";
                }
                else if (res > 0)
                {
                    // 收到 ACK
                    wait_for_ack();

                    /** 检查是否进入拥塞避免阶段 */
                    if (cwnd_ >= ssthresh_)
                    {
                        LOG(INFO) << "CHANGE TO CONG AVD";
                        is_cong_avd_ = true;
                        is_slow_start_ = false;

                        cwnd_ = 1;
                        ssthresh_ = 64;
                    }

                    /**
                     * 如果所有已发送数据包都被确认，则根据拥塞控制算法调整窗口大小
                     */
                    if (sliding_window_->last_acked_packet_ ==
                        sliding_window_->last_packet_sent_)
                    {
                        if (is_slow_start_)
                        {
                            cwnd_ = cwnd_ * 2; /** 慢启动阶段：指数增长 */
                        }
                        else
                        {
                            cwnd_ = cwnd_ + 1; /** 拥塞避免阶段：线性增长 */
                        }
                        break;
                    }
                }
                else
                {
                    // 超时
                    LOG(INFO) << "Timeout occurred SELECT::" << smoothed_timeout_;

                    /** 拥塞控制：慢启动阈值调整 */
                    ssthresh_ = cwnd_ / 2;
                    if (ssthresh_ < 1)
                    {
                        ssthresh_ = 1;
                    }
                    cwnd_ = 1;

                    /** 如果处于快速恢复阶段，则切换回慢启动 */
                    if (is_fast_recovery_)
                    {
                        is_fast_recovery_ = false;
                    }
                    is_slow_start_ = true;
                    is_cong_avd_ = false;

                    /**
                     * 重新传输所有未被确认的数据包
                     */
                    for (int i = sliding_window_->last_acked_packet_ + 1;
                         i <= sliding_window_->last_packet_sent_; i++)
                    {
                        int retransmit_start_byte = 0;
                        if (sliding_window_->last_acked_packet_ != -1)
                        {
                            retransmit_start_byte =
                                sliding_window_
                                ->sliding_window_buffers_[sliding_window_
                                    ->last_acked_packet_]
                                .first_byte_ +
                                MAX_DATA_SIZE;
                        }
                        LOG(INFO) << "Timeout Retransmit seq number"
                            << retransmit_start_byte + initial_seq_number_;
                        retransmit_segment(retransmit_start_byte);
                        packet_statistics_->retransmit_count_++; /** 统计重传次数 */
                        LOG(INFO) << "Timeout: retransmission at " << retransmit_start_byte;
                    }

                    break;
                }
            }
            LOG(INFO) << "SELECT END !!!";
            LOG(INFO) << "current byte ::" << start_byte_ << " file_length_ "
                << file_length_;
        }

        gettimeofday(&process_end_time, NULL); /** 记录结束时间 */

        /**
         * 计算整个传输过程的总时间
         */
        int64_t total_time =
            (process_end_time.tv_sec * 1000000 + process_end_time.tv_usec) -
            (process_start_time.tv_sec * 1000000 + process_start_time.tv_usec);

        int total_packet_sent = packet_statistics_->slow_start_packet_sent_count_ +
            packet_statistics_->cong_avd_packet_sent_count_;
        LOG(INFO) << "\n";
        LOG(INFO) << "========================================";
        LOG(INFO) << "Total Time: " << (float)total_time / pow(10, 6) << " secs";
        LOG(INFO) << "Statistics: 拥塞控制--慢启动: "
            << packet_statistics_->slow_start_packet_sent_count_
            << " 拥塞控制--拥塞避免: "
            << packet_statistics_->cong_avd_packet_sent_count_;
        LOG(INFO) << "Statistics: Slow start: "
            << ((float)packet_statistics_->slow_start_packet_sent_count_ /
                total_packet_sent) *
            100
            << "% CongAvd: "
            << ((float)packet_statistics_->cong_avd_packet_sent_count_ /
                total_packet_sent) *
            100
            << "%";
        LOG(INFO) << "Statistics: Retransmissions: "
            << packet_statistics_->retransmit_count_;
        LOG(INFO) << "========================================";
    }

    /**
     * 发送单个数据包，处理滑动窗口中的缓冲区更新，并准备发送数据。
     *
     * @param seq_number 数据包的序列号
     * @param start_byte 当前数据块在文件中的起始字节位置
     */
    void UdpServer::send_packet(int seq_number, int start_byte)
    {
        bool lastPacket = false; /** 标记是否是最后一个数据包 */
        int dataLength = 0; /** 定义本次发送的数据长度 */

        /**
         * 判断当前要发送的数据块是否是最后一个数据包
         * 如果剩余字节数小于等于最大数据长度，则表示这是最后一个包
         */
        if (file_length_ <= start_byte + MAX_DATA_SIZE)
        {
            LOG(INFO) << "Last packet to be sent !!!";
            dataLength = file_length_ - start_byte;
            lastPacket = true;
        }
        else
        {
            dataLength = MAX_DATA_SIZE; /** 否则按最大数据长度发送 */
        }

        struct timeval time;

        gettimeofday(&time, NULL); /** 获取当前时间戳，用于 RTT 计算 */

        /**
         * 如果已存在已发送但未确认的数据包，并且当前要发送的是之前已经发过的包（重传）
         * 则更新该数据包的时间戳
         */
        if (sliding_window_->last_packet_sent_ != -1 &&
            start_byte <
            sliding_window_
            ->sliding_window_buffers_[sliding_window_->last_packet_sent_]
            .first_byte_)
        {
            for (int i = sliding_window_->last_acked_packet_ + 1;
                 i < sliding_window_->last_packet_sent_; i++)
            {
                if (sliding_window_->sliding_window_buffers_[i].first_byte_ ==
                    start_byte)
                {
                    sliding_window_->sliding_window_buffers_[i].time_sent_ = time;
                    break;
                }
            }
        }
        else
        {
            /**
             * 否则将新数据包信息加入滑动窗口缓冲区
             */
            SlidWinBuffer slidingWindowBuffer;
            slidingWindowBuffer.first_byte_ = start_byte; /** 设置该数据包的起始字节位置 */
            slidingWindowBuffer.data_length_ = dataLength; /** 设置该数据包的数据长度 */
            slidingWindowBuffer.seq_num_ = initial_seq_number_ + start_byte; /** 设置序列号 */
            struct timeval time;
            gettimeofday(&time, NULL);
            slidingWindowBuffer.time_sent_ = time; /** 记录发送时间，用于 RTT 计算 */

            /**
             * 将新的缓冲区加入滑动窗口
             */
            sliding_window_->last_packet_sent_ =
                sliding_window_->AddToBuffer(slidingWindowBuffer);
        }

        /**
         * 从文件中读取指定范围的数据并发送
         */
        read_file_and_send(lastPacket, start_byte, start_byte + dataLength);
    }

    /**
     * 等待并处理客户端的 ACK 响应。
     */
    void UdpServer::wait_for_ack()
    {
        /** 定义接收缓冲区并初始化为 0 */
        unsigned char buffer[MAX_PACKET_SIZE];
        memset(buffer, 0, MAX_PACKET_SIZE);

        /** 客户端地址信息 */
        socklen_t addr_size;
        struct sockaddr_in client_address;
        addr_size = sizeof(client_address);

        int n = 0;
        int ack_number;

        /**
         * 循环接收数据直到成功收到数据包
         * recvfrom 用于接收 UDP 数据包
         */
        while ((n = recvfrom(sockfd_, buffer, MAX_PACKET_SIZE, 0,
                             (struct sockaddr*)&client_address, &addr_size)) <= 0)
        {
        };

        /** 反序列化接收到的数据为数据段对象 */
        DataSegment ack_segment;
        ack_segment.DeserializeToDataSegment(buffer, n);

        /** 获取最后一个已确认的数据包 */
        SlidWinBuffer last_packet_acked_buffer =
            sliding_window_
            ->sliding_window_buffers_[sliding_window_->last_acked_packet_];

        /** 如果收到 ACK 标志 */
        if (ack_segment.ack_flag_)
        {
            /**
             * 如果收到的是当前发送窗口基地址的 ACK，
             * 则视为重复 ACK（DUP ACK）
             */
            if (ack_segment.ack_number_ == sliding_window_->send_base_)
            {
                LOG(INFO) << "DUP ACK Received: ack_number: " << ack_segment.ack_number_;
                sliding_window_->dup_ack_++;

                /**
                 * 如果连续收到 3 次重复 ACK，则触发快速重传
                 */
                if (sliding_window_->dup_ack_ == 3)
                {
                    packet_statistics_->retransmit_count_++;
                    LOG(INFO) << "Fast Retransmit seq_number: " << ack_segment.ack_number_;
                    retransmit_segment(ack_segment.ack_number_ - initial_seq_number_);
                    sliding_window_->dup_ack_ = 0;

                    if (cwnd_ > 1)
                    {
                        cwnd_ = cwnd_ / 2;
                    }
                    ssthresh_ = cwnd_;
                    is_fast_recovery_ = true;
                }
            }
            /**
             * 如果收到新的 ACK，表示数据包已被正确接收
             */
            else if (ack_segment.ack_number_ > sliding_window_->send_base_)
            {
                /**
                 * 如果处于快速恢复阶段，则调整拥塞控制参数
                 */
                if (is_fast_recovery_)
                {
                    cwnd_++;
                    is_fast_recovery_ = false;
                    is_cong_avd_ = true;
                    is_slow_start_ = false;
                }

                sliding_window_->dup_ack_ = 0;
                sliding_window_->send_base_ = ack_segment.ack_number_;

                /**
                 * 如果是第一个已确认的数据包，则初始化相关变量
                 */
                if (sliding_window_->last_acked_packet_ == -1)
                {
                    sliding_window_->last_acked_packet_ = 0;
                    last_packet_acked_buffer =
                        sliding_window_
                        ->sliding_window_buffers_[sliding_window_->last_acked_packet_];
                }

                ack_number = last_packet_acked_buffer.seq_num_ +
                    last_packet_acked_buffer.data_length_;

                /**
                 * 更新已确认的数据包信息
                 */
                while (ack_number < ack_segment.ack_number_)
                {
                    sliding_window_->last_acked_packet_++;
                    last_packet_acked_buffer =
                        sliding_window_
                        ->sliding_window_buffers_[sliding_window_->last_acked_packet_];
                    ack_number = last_packet_acked_buffer.seq_num_ +
                        last_packet_acked_buffer.data_length_;
                }

                /**
                 * 计算 RTT 和超时时间
                 */
                struct timeval startTime = last_packet_acked_buffer.time_sent_;
                struct timeval endTime;
                gettimeofday(&endTime, NULL);
                calculate_rtt_and_time(startTime, endTime);
            }
        }
    }

    /**
     * 计算 RTT（往返时间）和超时时间。
     *
     * @param start_time RTT 开始时间
     * @param end_time RTT 结束时间
     */
    void UdpServer::calculate_rtt_and_time(struct timeval start_time,
                                           struct timeval end_time)
    {
        /** 如果开始时间为 0，则不进行计算 */
        if (start_time.tv_sec == 0 && start_time.tv_usec == 0)
        {
            return;
        }

        /** 计算样本 RTT（单位：微秒） */
        long sample_rtt = (end_time.tv_sec * 1000000 + end_time.tv_usec) -
            (start_time.tv_sec * 1000000 + start_time.tv_usec);

        /** 使用加权平均算法更新平滑 RTT */
        smoothed_rtt_ = smoothed_rtt_ + 0.125 * (sample_rtt - smoothed_rtt_);

        /** 更新 RTT 偏差 */
        dev_rtt_ = 0.75 * dev_rtt_ + 0.25 * (abs(smoothed_rtt_ - sample_rtt));

        /** 计算超时时间 */
        smoothed_timeout_ = smoothed_rtt_ + 4 * dev_rtt_;

        /** 如果超时时间过长，则随机设置一个较小值 */
        if (smoothed_timeout_ > 1000000)
        {
            smoothed_timeout_ = rand() % 30000;
        }
    }

    /**
     * 重新传输指定起始字节位置的数据段。
     *
     * @param index_number 要重传的数据段的起始字节位置
     */
    void UdpServer::retransmit_segment(int index_number)
    {
        /** 查找滑动窗口中需要重传的数据包并更新发送时间 */
        for (int i = sliding_window_->last_acked_packet_ + 1;
             i < sliding_window_->last_packet_sent_; i++)
        {
            if (sliding_window_->sliding_window_buffers_[i].first_byte_ ==
                index_number)
            {
                struct timeval time;
                gettimeofday(&time, NULL);
                sliding_window_->sliding_window_buffers_[i].time_sent_ = time;
                break;
            }
        }

        /** 从指定位置读取文件数据并发送 */
        read_file_and_send(false, index_number, index_number + MAX_DATA_SIZE);
    }

    /**
     * 从文件中读取指定范围的数据并发送到客户端。
     *
     * @param fin_flag 是否是最后一个数据包
     * @param start_byte 数据块在文件中的起始字节位置
     * @param end_byte 数据块在文件中的结束字节位置
     */
    void UdpServer::read_file_and_send(bool fin_flag, int start_byte,
                                       int end_byte)
    {
        int datalength = end_byte - start_byte;

        /** 如果剩余数据量小于最大数据长度，则这是最后一个数据包 */
        if (file_length_ - start_byte < datalength)
        {
            datalength = file_length_ - start_byte;
            fin_flag = true;
        }

        /** 分配内存用于存储读取的文件数据 */
        char* fileData = reinterpret_cast<char*>(calloc(datalength, sizeof(char)));

        /** 检查文件是否已经打开 */
        if (!file_.is_open())
        {
            LOG(ERROR) << "File open failed !!!";
            return;
        }

        /** 定位到文件中的指定位置并读取数据 */
        file_.seekg(start_byte);
        file_.read(fileData, datalength);

        /** 创建数据段对象并设置相关字段 */
        DataSegment* data_segment = new DataSegment();
        data_segment->seq_number_ = start_byte + initial_seq_number_;
        data_segment->ack_number_ = 0;
        data_segment->ack_flag_ = false;
        data_segment->fin_flag_ = fin_flag;
        data_segment->length_ = datalength;
        data_segment->data_ = fileData;

        /** 发送数据段 */
        send_data_segment(data_segment);
        LOG(INFO) << "Packet sent:seq number: " << data_segment->seq_number_;

        /** 释放分配的内存 */
        free(fileData);
        free(data_segment);
    }

    /**
     * 接收客户端请求并返回接收到的数据。
     *
     * @param client_sockfd 客户端 socket 文件描述符
     * @return 返回接收到的数据缓冲区指针
     */
    char* UdpServer::GetRequest(int client_sockfd)
    {
        /** 分配内存用于存储接收的请求数据 */
        char* buffer =
            reinterpret_cast<char*>(calloc(MAX_PACKET_SIZE, sizeof(char)));
        struct sockaddr_in client_address;
        socklen_t addr_size;

        /** 清空缓冲区 */
        memset(buffer, 0, MAX_PACKET_SIZE);

        /** 获取客户端地址长度 */
        addr_size = sizeof(client_address);

        /** 接收来自客户端的数据 */
        recvfrom(client_sockfd, buffer, MAX_PACKET_SIZE, 0,
                 (struct sockaddr*)&client_address, &addr_size);

        /** 记录接收到的请求信息 */
        LOG(INFO) << "***Request received is: " << buffer;

        /** 保存客户端地址，供后续发送数据使用 */
        cli_address_ = client_address;

        /** 返回接收到的数据缓冲区 */
        return buffer;
    }

    /**
     * 发送数据段到客户端。
     *
     * @param data_segment 要发送的数据段对象
     */
    void UdpServer::send_data_segment(DataSegment* data_segment)
    {
        /** 将数据段序列化为字节数组 */
        char* datagramChars = data_segment->SerializeToCharArray();

        /** 发送数据到客户端 */
        sendto(sockfd_, datagramChars, MAX_PACKET_SIZE, 0,
               (struct sockaddr*)&cli_address_, sizeof(cli_address_));

        /** 释放序列化后的数据内存 */
        free(datagramChars);
    }
} // namespace safe_udp
