#include <netdb.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <glog/logging.h>
/* 引入其他所需的自己写的头文件*/
#include "udp_client.h"
#include "data_segment.h"

namespace safe_udp
{
    /**
     * 构造函数，初始化客户端接收数据包的状态变量
     */
    UdpClient::UdpClient()
    {
        last_in_order_packet_ = -1; /**< 最后一个按序到达的数据包索引 */
        last_packet_received_ = -1; /**< 最后一个接收到的数据包索引 */
        fin_flag_received_ = false; /**< 是否接收到结束标志 FIN */
    }

    /**
     * 发送文件请求并接收文件内容
     * @param file_name 请求的文件名
     */
    void UdpClient::SendFileRequest(const std::string& file_name)
    {
        int n;
        int next_seq_expected;
        int segments_in_between = 0;
        initial_seq_number_ = 67; /**< 初始化起始序列号 */

        if (receiver_window_ == 0)
        {
            receiver_window_ = 100; /**< 设置默认接收窗口大小 */
        }

        /**
         * 分配接收缓冲区
         */
        unsigned char* buffer =
            (unsigned char*)calloc(MAX_PACKET_SIZE, sizeof(unsigned char));

        /**
         * 打印服务器地址信息
         */
        LOG(INFO) << "server_add::" << server_address_.sin_addr.s_addr;
        LOG(INFO) << "server_add_port::" << server_address_.sin_port;
        LOG(INFO) << "server_add_family::" << server_address_.sin_family;

        /**
         * 向服务器发送文件请求
         */
        n = sendto(sockfd_, file_name.c_str(), file_name.size(), 0,
                   (struct sockaddr*)&(server_address_), sizeof(struct sockaddr_in));
        if (n < 0)
        {
            LOG(ERROR) << "Failed to write to socket !!!";
        }
        memset(buffer, 0, MAX_PACKET_SIZE);

        /**
         * 打开本地文件准备写入
         */
        std::fstream file;
        std::string file_path = std::string(CLIENT_FILE_PATH) + file_name;
        file.open(file_path.c_str(), std::ios::out);

        /**
         * 循环接收数据包
         */
        while ((n = recvfrom(sockfd_, buffer, MAX_PACKET_SIZE, 0, NULL, NULL)) > 0)
        {
            char buffer2[20];
            memcpy(buffer2, buffer, 20);
            if (strstr("FILE NOT FOUND", buffer2) != NULL)
            {
                LOG(ERROR) << "File not found !!!";
                return;
            }

            /**
             * 反序列化数据包
             */
            std::unique_ptr<DataSegment> data_segment = std::make_unique<DataSegment>();
            data_segment->DeserializeToDataSegment(buffer, n);

            LOG(INFO) << "packet received with seq_number_:"
                << data_segment->seq_number_;

            /**
             * 模拟随机丢包
             */
            if (is_packet_drop_&& rand() %100 < prob_value_
            )
            {
                LOG(INFO) << "Dropping this packet with seq "
                    << data_segment->seq_number_;
                continue;
            }

            /**
             * 模拟随机延迟
             */
            if (is_delay_&& rand() %100 < prob_value_
            )
            {
                int sleep_time = (rand() % 10) * 1000;
                LOG(INFO) << "Delaying this packet with seq " << data_segment->seq_number_
                    << " for " << sleep_time << "us";
                usleep(sleep_time);
            }

            /**
             * 确定下一个期望的序列号
             */
            if (last_in_order_packet_ == -1)
            {
                next_seq_expected = initial_seq_number_;
            }
            else
            {
                next_seq_expected = data_segments_[last_in_order_packet_].seq_number_ +
                    data_segments_[last_in_order_packet_].length_;
            }

            /**
             * 处理旧数据包，直接发送 ACK
             */
            if (next_seq_expected > data_segment->seq_number_ &&
                !data_segment->fin_flag_)
            {
                send_ack(next_seq_expected);
                continue;
            }

            /**
             * 计算当前数据段在缓冲区中的索引
             */
            segments_in_between =
                (data_segment->seq_number_ - next_seq_expected) / MAX_DATA_SIZE;
            int this_segment_index = last_in_order_packet_ + segments_in_between + 1;

            /**
             * 判断是否超出接收窗口，超出则丢弃
             */
            if (this_segment_index - last_in_order_packet_ > receiver_window_)
            {
                LOG(INFO) << "Packet dropped " << this_segment_index;
                continue;
            }

            /**
             * 检查是否收到结束标志 FIN
             */
            if (data_segment->fin_flag_)
            {
                LOG(INFO) << "Fin flag received !!!";
                fin_flag_received_ = true;
            }

            /**
             * 将数据插入缓冲区
             */
            insert(this_segment_index, *data_segment);

            /**
             * 写入文件并更新已接收的最后一个有序包索引
             */
            for (int i = last_in_order_packet_ + 1; i <= last_packet_received_; i++)
            {
                if (data_segments_[i].seq_number_ != -1)
                {
                    if (file.is_open())
                    {
                        file << data_segments_[i].data_;
                        last_in_order_packet_ = i;
                    }
                }
                else
                {
                    break;
                }
            }

            /**
             * 如果所有数据包已接收且收到 FIN，结束接收
             */
            if (fin_flag_received_&& last_in_order_packet_
            ==
            last_packet_received_
            )
            {
                break;
            }

            /**
             * 发送 ACK 确认当前最后一个有序包
             */
            send_ack(data_segments_[last_in_order_packet_].seq_number_ +
                data_segments_[last_in_order_packet_].length_);

            /**
             * 清空缓冲区准备下一次接收
             */
            memset(buffer, 0, MAX_PACKET_SIZE);
        }

        /**
         * 释放缓冲区并关闭文件
         */
        free(buffer);
        file.close();
    }

    /**
     * 将数据段添加到数据段向量中
     * @param data_segment 待添加的数据段
     * @return 插入后向量中该元素的索引位置
     */
    int UdpClient::add_to_data_segment_vector(const DataSegment& data_segment)
    {
        data_segments_.push_back(data_segment); /**< 将数据段压入向量 */
        return data_segments_.size() - 1; /**< 返回插入位置的索引 */
    }

    /**
     * 发送 ACK 确认包给服务器
     * @param ackNumber 要确认的序列号
     */
    void UdpClient::send_ack(int ackNumber)
    {
        LOG(INFO) << "Sending an ack :" << ackNumber;
        int n = 0;

        /**
         * 创建一个新的 ACK 数据段
         */
        DataSegment* ack_segment = new DataSegment();
        ack_segment->ack_flag_ = true; /**< 设置 ACK 标志 */
        ack_segment->ack_number_ = ackNumber; /**< 设置确认号 */
        ack_segment->fin_flag_ = false; /**< 不是 FIN 包 */
        ack_segment->length_ = 0; /**< 数据长度为 0 */
        ack_segment->seq_number_ = 0; /**< 序列号为 0（ACK 包不需要） */

        /**
         * 序列化数据段为字节数组
         */
        char* data = ack_segment->SerializeToCharArray();

        /**
         * 发送 ACK 到服务器
         */
        n = sendto(sockfd_, data, MAX_PACKET_SIZE, 0,
                   (struct sockaddr*)&(server_address_), sizeof(struct sockaddr_in));
        if (n < 0)
        {
            LOG(INFO) << "Sending ack failed !!!";
        }

        free(data); /**< 释放序列化后的数据内存 */
    }

    /**
     * 创建 UDP 套接字并连接到指定的服务器
     * @param server_address 服务器地址
     * @param port 服务器端口
     */
    void UdpClient::CreateSocketAndServerConnection(
        const std::string& server_address, const std::string& port)
    {
        struct hostent* server;
        struct sockaddr_in server_address_;
        int sfd;
        int port_num;
        port_num = atoi(port.c_str()); /**< 将端口号转换为整数 */

        /**
         * 创建 UDP 套接字
         */
        sfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sfd < 0)
        {
            LOG(ERROR) << "Failed to socket !!!";
        }

        /**
         * 获取服务器主机信息
         */
        server = gethostbyname(server_address.c_str());
        if (server == NULL)
        {
            LOG(ERROR) << "No such host !!!";
            exit(0);
        }

        /**
         * 初始化服务器地址结构体
         */
        memset(&server_address_, 0, sizeof(server_address_));
        server_address_.sin_family = AF_INET; /**< 使用 IPv4 地址族 */
        memcpy(&server_address_.sin_addr.s_addr, server->h_addr, server->h_length); /**< 设置 IP 地址 */
        server_address_.sin_port = htons(port_num); /**< 设置端口号，转换为网络字节序 */

        sockfd_ = sfd; /**< 保存创建的套接字描述符 */
        this->server_address_ = server_address_; /**< 保存服务器地址信息 */
    }

    /**
     * 将数据段插入到缓冲区的指定索引位置
     * @param index 插入的目标索引
     * @param data_segment 要插入的数据段
     */
    void UdpClient::insert(int index, const DataSegment& data_segment)
    {
        if (index > last_packet_received_)
        {
            /**
             * 如果索引大于最后一个接收包索引，则逐个填充空段并添加新段
             */
            for (int i = last_packet_received_ + 1; i <= index; i++)
            {
                if (i == index)
                {
                    data_segments_.push_back(data_segment); /**< 添加目标数据段 */
                }
                else
                {
                    DataSegment data_segment; /**< 创建默认数据段填充空位 */
                    data_segments_.push_back(data_segment);
                }
            }
            last_packet_received_ = index; /**< 更新最后收到的数据包索引 */
        }
        else
        {
            data_segments_[index] = data_segment; /**< 替换已有索引位置的数据段 */
        }
    }
}