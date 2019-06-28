#include "CheckStream.h"


uint16_t crc16_update(uint16_t crc, uint16_t data) {
    data = data ^ (crc & 0xFF);
    data = data ^ ((data << 4) & 0xFF);
    return (((data << 8) & 0xFFFF) | ((crc >> 8) & 0xFF)) ^ (data >> 4) ^ (data << 3);
}
