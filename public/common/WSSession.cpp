#include "WSSession.h"
#include <vector>

#include <TracyProtocol.hpp>
#include <iostream>

#ifdef _WIN32
#include <winsock.h>
#endif

#include "../public/common/sha1.h"

namespace tracy
{

#define MIN(a, b) ((a) <= (b) ? (a) : (b))

uint64_t ntoh64(uint64_t x) {
  return ntohl(x>>32) | ((uint64_t)ntohl(x&0xFFFFFFFFu) << 32);
}
static const unsigned char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_encode(void *dst, const void *src, size_t len) { // thread-safe, re-entrant
    unsigned int *d = (unsigned int *)dst;
    const unsigned char *s = (const unsigned char*)src;
    const unsigned char *end = s + len;
    while (s < end) {
        uint32_t e = *s++ << 16;
        if (s < end) e |= *s++ << 8;
        if (s < end) e |= *s++;
        *d++ = b64[e >> 18] | (b64[(e >> 12) & 0x3F] << 8) | (b64[(e >> 6) & 0x3F] << 16) | (b64[e & 0x3F] << 24);
    }
    for (size_t i = 0; i < (3 - (len % 3)) % 3; i++) ((char *)d)[-1-i] = '=';
}

static int GetHttpHeader(const char *headers, const char *header, char *out, int maxBytesOut) { // thread-safe, re-entrant
    const char *pos = strstr(headers, header);
    if (!pos) return 0;
    pos += strlen(header);
    const char *end = pos;
    while (*end != '\r' && *end != '\n' && *end != '\0') ++end;
    int numBytesToWrite = MIN((int)(end-pos), maxBytesOut-1);
    memcpy(out, pos, numBytesToWrite);
    out[numBytesToWrite] = '\0';
    return (int)(end-pos);
}

bool SendHandshake(Socket* socket, const char *request) {
    const char webSocketGlobalGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // 36 characters long
    char key[128+sizeof(webSocketGlobalGuid)];
    GetHttpHeader(request, "Sec-WebSocket-Key: ", key, sizeof(key)/2);
    strcat(key, webSocketGlobalGuid);

    char sha1[21];
    SHA1(sha1, key, (int)strlen(key));

    char handshakeMsg[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: 0000000000000000000000000000\r\n"
      "\r\n";

    base64_encode(strstr(handshakeMsg, "Sec-WebSocket-Accept: ") + strlen("Sec-WebSocket-Accept: "), sha1, 20);

    size_t handshakeMsgSize = strlen(handshakeMsg);
    int error = socket->Send(handshakeMsg, handshakeMsgSize);
    if (error < 0)
    {
        return false;
    }
    return true;
}

bool WebSocketHasFullHeader(uint8_t *data, uint64_t obtainedNumBytes) {
    if (obtainedNumBytes < 2) return false;
    uint64_t expectedNumBytes = 2;
    WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
    if (header->mask) expectedNumBytes += 4;
    switch (header->payloadLength) {
    case 127: return expectedNumBytes += 8; break;
    case 126: return expectedNumBytes += 2; break;
    default: break;
    }
    return obtainedNumBytes >= expectedNumBytes;
}

uint64_t WebSocketFullMessageSize(uint8_t *data, uint64_t obtainedNumBytes) {
    uint64_t expectedNumBytes = 2;
    WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
    if (header->mask) expectedNumBytes += 4;
    switch (header->payloadLength) {
    case 127: return expectedNumBytes += 8 + ntoh64(*(uint64_t*)(data+2)); break;
    case 126: return expectedNumBytes += 2 + ntohs(*(uint16_t*)(data+2)); break;
    default: expectedNumBytes += header->payloadLength; break;
    }
    return expectedNumBytes;
}

uint64_t WebSocketMessagePayloadLength(uint8_t *data, uint64_t numBytes) {
    WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
    switch (header->payloadLength) {
    case 127: return ntoh64(*(uint64_t*)(data+2));
    case 126: return ntohs(*(uint16_t*)(data+2));
    default: return header->payloadLength;
    }
}

uint8_t *WebSocketMessageData(uint8_t *data, uint64_t numBytes) {
    WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
    data += 2; // Two bytes of fixed size header
    if (header->mask) data += 4; // If there is a masking key present in the header, that takes up 4 bytes
    switch (header->payloadLength) {
    case 127: return data + 8; // 64-bit length
    case 126: return data + 2; // 16-bit length
    default: return data; // 7-bit length that was embedded in fixed size header.
    }
}

void WebSocketMessageUnmaskPayload(uint8_t* payload,
                                   uint64_t payloadLength,
                                   uint32_t maskingKey) {
    uint8_t maskingKey8[4];
    memcpy(maskingKey8, &maskingKey, 4);
    uint32_t *data_u32 = (uint32_t *)payload;
    uint32_t *end_u32 = (uint32_t *)((uintptr_t)(payload + (payloadLength & ~3u)));

    while (data_u32 < end_u32)
        *data_u32++ ^= maskingKey;

    uint8_t *end = payload + payloadLength;
    uint8_t *data = (uint8_t *)data_u32;
    while (data < end) {
        *data ^= maskingKey8[(data-payload) % 4];
        ++data;
    }
}

uint32_t WebSocketMessageMaskingKey(uint8_t *data, uint64_t numBytes) {
    WebSocketMessageHeader *header = (WebSocketMessageHeader *)data;
    if (!header->mask) return 0;
    switch (header->payloadLength) {
    case 127: return *(uint32_t*)(data+10);
    case 126: return *(uint32_t*)(data+4);
    default: return *(uint32_t*)(data+2);
    }
}

int32_t WSSendMessage(Socket* socket, const void* buf, uint64_t numBytes)
{
  uint8_t headerData[sizeof(WebSocketMessageHeader) + 8/*possible extended length*/];
  memset(headerData, 0, sizeof(headerData));
  WebSocketMessageHeader *header = (WebSocketMessageHeader *)headerData;
  header->opcode = 0x02;
  header->fin = 1;
  int headerBytes = 2;

  if (numBytes < 126) {
    header->payloadLength = (unsigned int)numBytes;
  } else if (numBytes <= 65535) {
    header->payloadLength = 126;
    *(uint16_t*)(headerData+headerBytes) = htons((unsigned short)numBytes);
    headerBytes += 2;
  } else {
    header->payloadLength = 127;
    *(uint64_t*)(headerData+headerBytes) = hton64(numBytes);
    headerBytes += 8;
  }

  socket->Send((const char*)headerData, headerBytes); // header
  socket->Send((const char*)buf, (int)numBytes); // payload
  return 0;
}

bool WSRecvMessage2(Socket* socket, std::vector<uint8_t>& frameData, int timeout)
{
    int opcode = 0;
    int cur_len = 0;
    do
    {
        WebSocketMessageHeader ws_head;
        if (!socket->ReadRaw( &ws_head, sizeof(ws_head), timeout ))
        {
            continue;
        }

        if (ws_head.opcode == WebSocketMessageHeader::OPCODE::CLOSE)
        {
            break;
        }
        else if(ws_head.opcode == WebSocketMessageHeader::OPCODE::CONTINUE
            || ws_head.opcode == WebSocketMessageHeader::OPCODE::TEXT_FRAME
            || ws_head.opcode == WebSocketMessageHeader::OPCODE::BIN_FRAME)
        {
            uint64_t length = 0;
            if (ws_head.payloadLength == 126)
            {
                uint16_t len = 0;
                if (!socket->ReadRaw( &len, sizeof(len), timeout ))
                {
                    break;
                }
                length = ntohs(len);
            } else if (ws_head.payloadLength == 127)
            {
                uint64_t len = 0;
                if (!socket->ReadRaw( &len, sizeof(len), timeout))
                {
                    break;
                }
                length = ntoh64(len);
            } else
            {
                length = ws_head.payloadLength;
            }

            char mask[4] = {};
            if (ws_head.mask)
            {
                if (!socket->ReadRaw( mask, sizeof(mask), timeout ))
                {
                    break;
                }
            }
            frameData.resize(cur_len + length);
            if (!socket->ReadRaw( &frameData[cur_len], length, timeout ))
            {
                break;
            }
            if(ws_head.mask) {
                for(int i = 0; i < (int)length; ++i) {
                    frameData[cur_len + i] ^= mask[i % 4];
                }
            }
            cur_len += length;

            if(!opcode && ws_head.opcode != WebSocketMessageHeader::OPCODE::CONTINUE) {
                opcode = ws_head.opcode;
            }
            if (ws_head.fin)
            {
                return true;
            }
        }
    } while(true);
    return false;
}

bool WSRecvMessage(Socket* socket, void* buf, int len, int timeout)
{
    const int maxBufSize = TargetFrameSize;
    int maxLen = maxBufSize;
    char tmpBuf[maxBufSize];
    bool read = false;
    while (maxLen == maxBufSize)
    {
        read = socket->ReadMax(tmpBuf, maxLen, timeout);
    }
    if (!read)
    {
        return false;
    }
    std::vector<uint8_t> fragmentData;
    fragmentData.insert(fragmentData.end(), tmpBuf, tmpBuf + maxBufSize - maxLen);
    while (!fragmentData.empty())
    {
        bool hasFullHeader = WebSocketHasFullHeader(&fragmentData[0], fragmentData.size());
        if (!hasFullHeader) {
            break;
        }
        uint64_t neededBytes = WebSocketFullMessageSize(&fragmentData[0], fragmentData.size());
        if (fragmentData.size() < neededBytes) {
            break;
        }

        WebSocketMessageHeader *header = (WebSocketMessageHeader *)&fragmentData[0];
        if (header->opcode == 0x8)
        {
            return false;
        }
        uint64_t payloadLength = WebSocketMessagePayloadLength(&fragmentData[0], neededBytes);
        uint8_t *payload = WebSocketMessageData(&fragmentData[0], neededBytes);

        if (header->mask)
            WebSocketMessageUnmaskPayload(payload, payloadLength, WebSocketMessageMaskingKey(&fragmentData[0], neededBytes));
        if (len >= payloadLength) {
            memcpy( buf, payload, payloadLength );
            return true;
        }
        fragmentData.erase(fragmentData.begin(), fragmentData.begin() + (ptrdiff_t)neededBytes);
    }
    return false;
}

}