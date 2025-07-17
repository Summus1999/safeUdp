#include "packet_statistics.h"

namespace safe_udp {
/**
 * 构造函数，初始化各类数据包统计计数器为 0
 */
PacketStatistics::PacketStatistics() {
  slowStartPacketTxStatistics = 0; /**< 慢启动阶段已发送的数据包计数初始化 */
  congAvdPacketTxStatistics = 0;   /**< 拥塞避免阶段已发送的数据包计数初始化 */
  slowStartPacketRxStatistics = 0; /**< 慢启动阶段接收到的数据包计数初始化 */
  congAvdPacketRxStatistics = 0;   /**< 拥塞避免阶段接收到的数据包计数初始化 */
  retransStatistics = 0;           /**< 重传数据包计数初始化 */
}

/**
 * 析构函数
 */
PacketStatistics::~PacketStatistics() {}
}  // namespace safe_udp
