#include <stdint.h>
#include <string.h>
extern "C" int32_t probe_bytes(const uint8_t* data, int32_t len) {
    if (data && len == 5 && data[0]==0x61 && data[1]==0x00 && data[2]==0x62 && data[3]==0xFF && data[4]==0x80) return 42;
    return -1;
}
