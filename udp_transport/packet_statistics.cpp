#include "packet_statistics.h"

namespace safe_udp
{
/**
 * 构造函数，初始化各类数据包统计计数器为 0
 */
PacketStatistics::PacketStatistics()
{
  slow_start_packet_sent_count_ = 0; /**< 慢启动阶段已发送的数据包计数初始化 */
  cong_avd_packet_sent_count_ = 0; /**< 拥塞避免阶段已发送的数据包计数初始化 */
  slow_start_packet_rx_count_ = 0; /**< 慢启动阶段接收到的数据包计数初始化 */
  cong_avd_packet_rx_count_ = 0; /**< 拥塞避免阶段接收到的数据包计数初始化 */
  retransmit_count_ = 0; /**< 重传数据包计数初始化 */
}

/**
 * 析构函数
 */
PacketStatistics::~PacketStatistics()
{
}
} // namespace safe_udp
