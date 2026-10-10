#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t id_recipient;
    uint8_t id_sender;
    uint8_t type_paket;
    uint16_t flags;
    uint8_t num_packet;
    uint8_t crc;
    uint8_t payload_len;
    uint8_t payload[];

} packet_handle_t;
