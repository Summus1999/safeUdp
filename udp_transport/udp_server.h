#pragma once
#include <netinet/in.h>     // 定义 sockaddr_in 结构体等网络相关数据结构
#include <unistd.h>         // 提供 POSIX 操作系统 API 的访问，如 close()
#include <fstream>          // 文件流，用于读写文件
#include <iostream>         // 输入输出流，用于打印调试信息等
#include <memory>           // 智能指针支持，如 unique_ptr
#include <string>           // 使用 std::string 存储字符串数据
/*自定义头文件实现数据*/
#include "data_segment.h"       // 自定义头文件：数据分段类定义
#include "packet_statistics.h"  // 自定义头文件：统计发送/接收的数据包信息
#include "sliding_window.h"     // 自定义头文件：滑动窗口机制实现

namespace safe_udp {

/**
 * UdpServer 类
 * 实现一个基于 UDP 协议的服务器，提供可靠的数据传输功能，包括流量控制、拥塞控制、RTT 测量等。
 */
class UdpServer {
 public:
  /**
   * 构造函数
   * 初始化成员变量，建立连接前的准备操作（具体初始化可能在 StartServer 中进行）
   */
  UdpServer();

  /**
   * 析构函数
   * 关闭 socket 和打开的文件流，释放资源
   */
  ~UdpServer() {
    close(sockfd_);
    file_.close();
  }

  /**
   * 接收客户端请求
   * @param client_sockfd 客户端 socket 描述符
   * @return 返回接收到的请求数据指针
   */
  char *GetRequest(int client_sockfd);

  /**
   * 打开指定文件
   * @param file_name 要打开的文件名
   * @return 成功打开返回 true，否则 false
   */
  bool OpenFile(const std::string &file_name);

  /**
   * 开始文件传输流程
   */
  void StartFileTransfer();

  /**
   * 向客户端发送错误信息
   */
  void SendError();

  /**
   * 拥塞控制与流量控制相关变量
   */
  int rwnd_;            // 接收窗口大小（Receiver Window）
  int cwnd_;            // 拥塞窗口大小（Congestion Window）
  int ssthresh_;        // 慢启动阈值（Slow Start Threshold）
  int start_byte_;      // 当前传输起始字节位置
  bool is_slow_start_;  // 是否处于慢启动阶段
  bool is_cong_avd_;    // 是否处于拥塞避免阶段
  bool is_fast_recovery_; // 是否处于快速恢复阶段
  int StartServer(int port); // 启动服务器，绑定指定端口并监听

 private:
  /**
   * 组件对象
   */
  std::unique_ptr<SlidingWindow> sliding_window_;      // 滑动窗口管理器
  std::unique_ptr<PacketStatistics> packet_statistics_; // 数据包统计工具

  /**
   * 私有成员变量
   */
  int sockfd_;                    // 服务器 socket 描述符
  std::fstream file_;             // 文件流对象
  struct sockaddr_in cli_address_;// 客户端地址结构体
  int initial_seq_number_;        // 初始序列号
  int file_length_;               // 文件总长度（字节数）
  double smoothed_rtt_;           // 平滑往返时间（Smoothed RTT）
  double dev_rtt_;                // RTT 偏差（Deviation RTT）
  double smoothed_timeout_;       // 平滑超时时间（Smoothed Timeout）

  /**
   * 内部方法声明
   */
  void send(); // 发送数据主逻辑

  /**
   * 发送指定序号和起始字节的数据包
   * @param seq_number 序列号
   * @param start_byte 起始字节位置
   */
  void sendpacket(int seq_number, int start_byte);

  /**
   * 计算 RTT（往返时间）及超时时间
   * @param start_time 请求开始时间
   * @param end_time 响应结束时间
   */
  void calculateRttAndTime(struct timeval start_time,
                              struct timeval end_time);

  /**
   * 重传指定索引的数据段
   * @param index_number 数据段索引
   */
  void retransmitSegment(int index_number);

  /**
   * 从文件中读取数据并发送
   * @param fin_flag 是否是最后一个数据段
   * @param start_byte 起始字节
   * @param end_byte 结束字节
   */
  void readFileAndSend(bool fin_flag, int start_byte, int end_byte);

  /**
   * 发送数据段
   * @param data_segment 待发送的数据段对象
   */
  void sendDataSegment(DataSegment *data_segment);

  /**
   * 等待客户端 ACK 回复
   */
  void waitForAck();
};
}  // namespace safe_udp