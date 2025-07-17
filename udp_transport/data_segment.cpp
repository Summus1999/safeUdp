#include "data_segment.h"

#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <string>

namespace safe_udp {
/**
 * 构造函数，初始化数据段的基本字段
 */
DataSegment::DataSegment() {
  ackNum = -1;     /**< 初始化确认号为 -1 */
  seqNumber = -1;  /**< 初始化序列号为 -1 */
  dataLength = -1; /**< 初始化数据长度为 -1 */
}

/**
 * 将数据段序列化为字符数组，用于网络传输
 * @return 返回序列化后的字节流指针
 */
char* DataSegment::SerializeToCharArray() {
  if (finalDataPacket != nullptr) {
    memset(finalDataPacket, 0, MAX_PACKET_SIZE); /**< 如果缓冲区已存在，清空 */
  } else {
    /**
     * 分配新的缓冲区内存
     */
    finalDataPacket =
        reinterpret_cast<char*>(calloc(MAX_PACKET_SIZE, sizeof(char)));
    if (finalDataPacket == nullptr) {
      return nullptr; /**< 内存分配失败 */
    }
  }

  /**
   * 将序列号写入数据包头部（4字节）
   */
  memcpy(finalDataPacket, &seqNumber, sizeof(seqNumber));

  /**
   * 将确认号写入数据包头部（4字节）
   */
  memcpy(finalDataPacket + 4, &ackNum, sizeof(ackNum));

  /**
   * 写入 ACK 标志（1字节）
   */
  memcpy((finalDataPacket + 8), &ackFlag, 1);

  /**
   * 写入 FIN 标志（1字节）
   */
  memcpy((finalDataPacket + 9), &finflag, 1);

  /**
   * 写入数据长度（2字节）
   */
  memcpy((finalDataPacket + 10), &dataLength, sizeof(dataLength));

  /**
   * 写入实际数据
   */
  memcpy((finalDataPacket + 12), data_, dataLength);

  return finalDataPacket;
}

/**
 * 从字节流反序列化为 DataSegment 对象
 * @param data_segment 数据包字节流
 * @param length 数据长度
 */
void DataSegment::DeserializeToDataSegment(unsigned char* data_segment,
                                           int length) {
  seqNumber = convert_to_uint32(data_segment, 0);   /**< 提取序列号 */
  ackNum = convert_to_uint32(data_segment, 4);      /**< 提取确认号 */
  ackFlag = convert_to_bool(data_segment, 8);       /**< 提取 ACK 标志 */
  finflag = convert_to_bool(data_segment, 9);       /**< 提取 FIN 标志 */
  dataLength = convert_to_uint16(data_segment, 10); /**< 提取数据长度 */

  /**
   * 分配内存存储数据部分
   */
  data_ = reinterpret_cast<char*>(calloc(length + 1, sizeof(char)));
  if (data_ == nullptr) {
    return;
  }

  /**
   * 拷贝数据部分到 data_ 成员变量
   */
  memcpy(data_, data_segment + HEADER_LENGTH, length);
  *(data_ + length) = '\0'; /**< 添加字符串结束符 */
}

/**
 * 将字节流中的4字节转换为 uint32_t 类型
 * @param buffer 字节流
 * @param start_index 起始索引
 * @return 转换后的 uint32_t 值
 */
uint32_t DataSegment::convert_to_uint32(unsigned char* buffer,
                                        int start_index) {
  uint32_t uint32_value =
      (buffer[start_index + 3] << 24) | (buffer[start_index + 2] << 16) |
      (buffer[start_index + 1] << 8) | (buffer[start_index]);
  return uint32_value;
}

/**
 * 将字节流中的2字节转换为 uint16_t 类型
 * @param buffer 字节流
 * @param start_index 起始索引
 * @return 转换后的 uint16_t 值
 */
uint16_t DataSegment::convert_to_uint16(unsigned char* buffer,
                                        int start_index) {
  uint16_t uint16_value =
      (buffer[start_index + 1] << 8) | (buffer[start_index]);
  return uint16_value;
}

/**
 * 将字节流中的1字节转换为 bool 类型
 * @param buffer 字节流
 * @param index 索引位置
 * @return 转换后的 bool 值
 */
bool DataSegment::convert_to_bool(unsigned char* buffer, int index) {
  bool bool_value = buffer[index];
  return bool_value;
}
}  // namespace safe_udp