#pragma once
#include <vector>
#include "buffer.h"

namespace safe_udp
{
/** 滑动窗口类，用于管理数据包的发送与确认 */
class SlidingWindow
{
public:
  /** 构造函数 */
  SlidingWindow();
  /** 析构函数 */
  ~SlidingWindow();

  /** 将数据包添加到滑动窗口缓冲区 */
  int AddToBuffer(const SlidWinBuffer& buffer);

  /** 存储滑动窗口中的数据包缓冲区 */
  std::vector<SlidWinBuffer> sliding_window_buffers_;
  /** 最后一个发送的数据包的序列号 */
  int last_packet_sent_;
  /** 最后一个被确认的数据包的序列号 */
  int last_acked_packet_;
  /** 成功发送且已经确认的数据包中最小的序列号 */
  int send_base_;
  /** 重复确认的数量 */
  int dup_ack_;
};
} // namespace safe_udp