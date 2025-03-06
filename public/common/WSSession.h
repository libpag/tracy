#ifndef __WSSESSIONG_H__
#define __WSSESSIONG_H__

#include <stdint.h>
#include <string>
#include <vector>
#include "TracySocket.hpp"

namespace tracy
{
uint64_t ntoh64(uint64_t x);
#define hton64 ntoh64

typedef struct
#if defined(__GNUC__)
__attribute__ ((packed, aligned(1)))
#endif
WebSocketMessageHeader {
  enum OPCODE
  {
    /// 数据分片帧
    CONTINUE = 0,
    /// 文本帧
    TEXT_FRAME = 1,
    /// 二进制帧
    BIN_FRAME = 2,
    /// 断开连接
    CLOSE = 8,
    /// PING
    PING = 0x9,
    /// PONG
    PONG = 0xA
  };
  unsigned opcode : 4;
  unsigned rsv : 3;
  unsigned fin : 1;
  unsigned payloadLength : 7;
  unsigned mask : 1;
} WebSocketMessageHeader;

bool SendHandshake(Socket* socket, const char *request);
bool WSRecvMessage2(Socket* socket, std::vector<uint8_t>& frameData, int timeout);
bool WSRecvMessage(Socket* socket, void* buf, int len, int timeout);
int32_t WSSendMessage(Socket* socket, const void* buf, uint64_t numBytes);

}

#endif