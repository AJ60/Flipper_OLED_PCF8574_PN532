#include "xiaomi.h"
#include "_protocols.h"

static const char* get_name(const Payload* payload) {
    UNUSED(payload);
    return "Xiaomi";
}

static void make_packet(uint8_t* _size, uint8_t** _packet, Payload* payload) {
    UNUSED(payload);
    uint8_t size = 28;
    uint8_t* packet = malloc(size);
    uint8_t i = 0;
    
    packet[i++] = 0x1B; // AD Length (27 bytes following)
    packet[i++] = 0xFF; // AD Type: Manufacturer Specific
    packet[i++] = 0x8F; // Xiaomi Company ID (0x038F LE)
    packet[i++] = 0x03; // ...
    packet[i++] = 0x16; // Prefix
    packet[i++] = 0x01; // ...
    packet[i++] = 0x20; // ...
    furi_hal_random_fill_buf(&packet[i], 2); i += 2; // Random 2 bytes
    packet[i++] = 0x17; // Constant middle
    packet[i++] = 0x0A;
    packet[i++] = 0x00;
    packet[i++] = 0x00;
    packet[i++] = 0x00;
    packet[i++] = 0x00;
    packet[i++] = 0x88;
    packet[i++] = 0x50;
    packet[i++] = 0x11;
    packet[i++] = 0xB1;
    packet[i++] = 0xFF;
    furi_hal_random_fill_buf(&packet[i], 2); i += 2; // Random 2 bytes
    memset(&packet[i], 0, 6); i += 6; // Suffix 6 zeros
    
    *_size = size;
    *_packet = packet;
}

const Protocol protocol_xiaomi = {
    .icon = &I_android,
    .get_name = get_name,
    .make_packet = make_packet,
    .extra_config = NULL,
    .config_count = NULL,
};
