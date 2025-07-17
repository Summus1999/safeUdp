#pragma once
#include <sys/time.h>
#include <time.h>

namespace safe_udp {
/*用于在 UDP 通信中管理滑动窗口缓冲区的类*/
class SlidWinBuffer {
 public:
  SlidWinBuffer() {}

  ~SlidWinBuffer() {}

  /*表示缓冲区中第一个字节的序列号*/
  int firstByteSeq;
  /*表示缓冲区中有效数据的长度*/
  int dataLength;
  /*表示当前数据包的序列号*/
  int currSeqNum;
  /*表示数据发送的时间戳，用于跟踪传输时间*/
  struct timeval timeSentStamp;
};
}  // namespace safe_udp
