#pragma once

namespace safe_udp {
/**
 * PacketStatistics 类用于统计不同状态下的数据包发送、接收及重传次数。
 */
class PacketStatistics {
 public:
  /**
   * 构造函数，初始化各个统计计数器。
   */
  PacketStatistics();

  /**
   * 析构函数，虚拟析构以支持继承。
   */
  virtual ~PacketStatistics();

  int slowStartPacketTxStatistics; /**< 慢启动阶段已发送的数据包数量 */
  int congAvdPacketTxStatistics;   /**< 拥塞避免阶段已发送的数据包数量 */
  int slowStartPacketRxStatistics; /**< 慢启动阶段接收到的数据包数量 */
  int congAvdPacketRxStatistics;   /**< 拥塞避免阶段接收到的数据包数量 */
  int retransStatistics;           /**< 数据包重传总次数 */
};
}  // namespace safe_udp