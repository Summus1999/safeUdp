#pragma once
#include <cstdint>
#include <cstdlib>

namespace safe_udp {
/* 定义最大数据包大小为1472字节 */
constexpr int MAX_PACKET_SIZE = 1472;
/* 定义最大数据载荷大小为1460字节 */
constexpr int MAX_DATA_SIZE = 1460;
/* 定义协议头部长度为12字节 */
constexpr int HEADER_LENGTH = 12;

/* DataSegment 类用于处理UDP传输中的数据分段，包括序列化与反序列化操作 */
class DataSegment {
 public:
  /* 构造函数，初始化资源 */
  DataSegment();
  /* 析构函数，释放分配的内存 */
  ~DataSegment() {
    if (!finalDataPacket) {
      free(finalDataPacket);
    }
  }

  /* 将数据段序列化为字符数组，供网络传输使用 */
  char* SerializeToCharArray();
  /* 从接收到的数据反序列化填充当前数据段对象 */
  void DeserializeToDataSegment(unsigned char* data_segment, int length);

  /* 数据段的序列号 */
  int seqNumber;
  /* 确认号，用于确认收到的数据段 */
  int ackNum;
  /* 标志位，表示该数据段是否包含确认信息 */
  bool ackFlag;
  /* 标志位，表示是否为结束数据段 */
  bool finflag;
  /* 数据段总长度 */
  uint16_t dataLength;
  /* 指向实际数据的指针，默认初始化为空 */
  char* data_ = nullptr;

 private:
  /* 从缓冲区指定位置提取32位无符号整数 */
  uint32_t convert_to_uint32(unsigned char* buffer, int start_index);
  /* 从缓冲区指定位置提取布尔值 */
  bool convert_to_bool(unsigned char* buffer, int index);
  /* 从缓冲区指定位置提取16位无符号整数 */
  uint16_t convert_to_uint16(unsigned char* buffer, int start_index);
  /* 存储序列化后的最终数据包 */
  char* finalDataPacket = nullptr;
};
}  // namespace safe_udp