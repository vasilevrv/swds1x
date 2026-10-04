#include "app_cfg.h"
#if (__PROJECT_TL_RELAY_2CH__)
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "relay_2ch.h"
#include "app_main.h"

/* Validate wire data before the SDK parsers, which assume complete payloads. */
static s16 valueSize(u8 type, const u8 *data, u16 remaining)
{
    u32 size;
    if (type == 0) size = 0;
    else if ((type >= 0x08 && type <= 0x0F) || (type >= 0x18 && type <= 0x2F)) size = (type & 7) + 1;
    else if (type == 0x10 || type == 0x30) size = 1;
    else if (type == 0x31 || type == 0x38 || type == 0xE8 || type == 0xE9) size = 2;
    else if (type == 0x39 || (type >= 0xE0 && type <= 0xE2) || type == 0xEA) size = 4;
    else if (type == 0x3A || type == 0xF0) size = 8;
    else if (type == 0xF1) size = 16;
    else if (type == 0x41 || type == 0x42) {
        if (!remaining) return -1;
        size = 1UL + data[0];
    } else if (type == 0x43 || type == 0x44) {
        if (remaining < 2) return -1;
        size = 2UL + data[0] + ((u32)data[1] << 8);
    } else return -1; /* No application attribute uses compound data types. */
    return size <= remaining ? (s16)size : -1;
}

static bool attributeRecords(u16 cluster, u8 command, const u8 *data, u16 length)
{
    while (length) {
        if (length < 3) return FALSE;
        u16 attribute = (u16)data[0] | ((u16)data[1] << 8);
        u8 type = data[2];
        data += 3; length -= 3;
        if (command == 1) { /* Read Attributes Response: status precedes type. */
            if (type) continue;
            if (!length) return FALSE;
            type = *data++; length--;
        }
        s16 size = valueSize(type, data, length);
        if (size < 0) return FALSE;
        if ((command == 2 || command == 3 || command == 5) &&
            cluster == 0x0006 && attribute == 0x4003 && type == 0x30 &&
            data[0] > 2 && data[0] != 0xFF) return FALSE;
        if ((command == 2 || command == 3 || command == 5) &&
            type == 0x10 && data[0] > 1) return FALSE;
        data += size; length -= size;
    }
    return TRUE;
}

static bool reportingRecords(const u8 *data, u16 length)
{
    while (length) {
        if (length < 3) return FALSE;
        u8 direction = data[0];
        data += 3; length -= 3;
        if (direction == 1) {
            if (length < 2) return FALSE;
            data += 2; length -= 2;
        } else if (direction == 0) {
            if (length < 5) return FALSE;
            u8 type = data[0];
            data += 5; length -= 5;
            /* Analog reporting changes have the same fixed size as the attribute. */
            bool analog = (type >= 0x20 && type <= 0x2F) ||
                          (type >= 0x38 && type <= 0x3A) ||
                          (type >= 0xE0 && type <= 0xE2);
            s16 size = valueSize(type, data, length);
            if (analog) {
                if (size < 0) return FALSE;
                data += size; length -= size;
            } else if (type == 0x41 || type == 0x42 || type == 0x43 || type == 0x44 ||
                       type == 0x48 || type == 0x4C || type == 0x50 || type == 0x51) return FALSE;
        } else return FALSE;
    }
    return TRUE;
}

static bool scenePayload(const u8 *data, u16 length)
{
    if (length < 6 || data[5] > length - 6) return FALSE;
    u16 offset = 6 + data[5];
    if (length - offset > ZCL_MAX_SCENE_EXT_FIELD_SIZE) return FALSE;
    while (offset < length) {
        if (length - offset < 3 || data[offset + 2] > length - offset - 3) return FALSE;
        offset += 3 + data[offset + 2];
    }
    return TRUE;
}

bool relay2_zclPacketValid(u16 cluster, const u8 *frame, u16 length)
{
    if (length < 3) return FALSE;
    u8 headerSize = (frame[0] & 4) ? 5 : 3;
    if (length < headerSize || (frame[0] & 3) > 1) return FALSE;
    u8 command = frame[headerSize - 1];
    bool serverToClient = (frame[0] & 8) != 0;
    const u8 *data = frame + headerSize;
    length -= headerSize;
    if (!(frame[0] & 3)) {
        if ((command == 2 || command == 3 || command == 5 || command == 6) && serverToClient) return FALSE;
        switch (command) {
        case 0: return length >= 2 && !(length & 1);
        case 1: case 2: case 3: case 5: case 0x0A:
            return length && attributeRecords(cluster, command, data, length);
        case 4: case 7:
            if (!length) return FALSE;
            if (data[0] == 0) return length == 1;
            while (length) {
                u8 status = *data++; length--;
                if (!status) return FALSE;
                u8 fields = command == 4 ? 2 : 3;
                if (fields > length) return FALSE;
                data += fields; length -= fields;
            }
            return TRUE;
        case 6: return length && reportingRecords(data, length);
        case 8: return length && !(length % 3);
        case 0x0B: return length == 2;
        case 0x0C: case 0x15: return length == 3;
        case 0x0D: return length >= 1 && !((length - 1) % 3);
        case 0x16: return length >= 1 && !((length - 1) % 4);
        default: return FALSE; /* No matching outgoing request or supported command. */
        }
    }
    if (cluster == 0x0019) { /* OTA client receives server-to-client commands. */
        if (!serverToClient || !length) return FALSE;
        if (command == 0) return data[0] <= 3 && length == (u16)(2 + (data[0] >= 1 ? 2 : 0) + (data[0] >= 2 ? 2 : 0) + (data[0] >= 3 ? 4 : 0));
        if (command == 2 || command == 9) return length == (data[0] == 0 ? 13 : 1);
        if (command == 5) {
            if (data[0] == 0) return length >= 14 && length == (u16)(14 + data[13]);
            return length == (data[0] == 0x97 ? 11 : 1);
        }
        return command == 7 && length == 16;
    }
    if (serverToClient) return FALSE;
    switch (cluster) {
    case 0x0000: return command == 0 && length == 0;
    case 0x0003: return (command == 0 || command == 0x40) ? length == 2 : command == 1 && length == 0;
    case 0x0006:
        if (command <= 2 || command == 0x41) return length == 0;
        if (command == 0x40) return length == 2;
        return command == 0x42 && length == 5;
    case 0x0004:
        if (command == 0 || command == 5) return length >= 3 && length == (u16)(3 + data[2]);
        if (command == 1 || command == 3) return length == 2;
        if (command == 2) return length >= 1 && length == (u16)(1 + 2 * data[0]);
        return command == 4 && length == 0;
    case 0x0005:
        if (command == 0 || command == 0x40) return scenePayload(data, length);
        if (command == 1 || command == 2 || command == 4 || command == 0x41) return length == 3;
        if (command == 3 || command == 6) return length == 2;
        if (command == 5) return length == 3 || length == 5;
        return command == 0x42 && length == 7;
    default: return FALSE; /* Config/profile commands are not implemented. */
    }
}

void relay2_zclRxHandler(void *data)
{
    apsdeDataInd_t *message = (apsdeDataInd_t *)data;
    if (!gRelayCtx.factoryResetPending &&
        relay2_zclPacketValid(message->indInfo.cluster_id, message->asdu, message->asduLen)) {
        zcl_rx_handler(data);
        return;
    }
    /* Rejected buffers are owned by the endpoint callback just like SDK RX. */
    ev_buf_free(data);
}
#endif
