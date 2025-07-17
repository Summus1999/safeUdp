#include "sliding_window.h"

namespace safe_udp
{
/**
 * 构造函数，初始化滑动窗口相关状态变量
 */
SlidingWindow::SlidingWindow()
{
  lastSendPacketSeq = -1; /**< 最后一个已发送的数据包索引 */
  lastAckedPacketSeq = -1; /**< 最后一个被确认的数据包索引 */
  sendBaseSeq = -1; /**< 当前发送窗口的基序号 */
  dupAckNum = 0; /**< 重复 ACK 计数，用于快速重传判断 */
}

/**
 * 析构函数
 */
SlidingWindow::~SlidingWindow()
{
}

/**
 * 将数据缓冲区添加到滑动窗口缓冲区列表中
 * @param buffer 待添加的滑动窗口缓冲区对象
 * @return 返回添加后的索引位置
 */
int SlidingWindow::AddToBuffer(const SlidWinBuffer& buffer)
{
  sliding_window_buffers_.push_back(buffer); /**< 将缓冲区压入向量 */
  return (sliding_window_buffers_.size() - 1); /**< 返回插入位置的索引 */
}
} // namespace safe_udp