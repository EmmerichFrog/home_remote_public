#include "ble_beacon.h"
#include "app.h"

/**
 * @brief      Check if sending a request is allowed
 * @param      model  the current model
 * @return     true if it's allowed
*/
bool allow_cmd_bt(BtBeacon* ble) {
    return ble->status != BEACON_BUSY;
}

/**
 * @brief      Callback of the timer_reset to update the btton pressed graphics.
 * @details    This function is called when the timer_reset ticks.
 * @param      context  The context - App object.
*/
void timer_beacon_reset_callback(void* context) {
    App* app = (App*)context;
    furi_thread_flags_set(app->comm_thread_id, ThreadCommStopCmd);
}

bool make_packet(BtBeacon* ble, uint8_t* _size, uint8_t** _packet) {
    uint8_t* packet = malloc(EXTRA_BEACON_MAX_DATA_SIZE);
    size_t i = 0;
    ble->cnt++;

    // Flag data
    packet[i++] = 0x02; // length
    packet[i++] = 0x03; // Type: Flags
    packet[i++] =
        0b00000110; // bit 1: “LE General Discoverable Mode”, bit 2: “BR/EDR Not Supported”
    // Service data
    packet[i++] = 0x08; // length
    packet[i++] = 0x16; // Type: Flags
    // BTHome Data
    packet[i++] = 0xD2; // UUID 1
    packet[i++] = 0xFC; // UUID 2
    packet[i++] = 0b01000100; // BTHome Device Information
    // Packet Id
    packet[i++] = 0x00; // Type: Packet ID
    packet[i++] = ble->cnt; // Packet Counter
    // Actual Data
    packet[i++] = 0x3A; // Type: Object ID Button
    packet[i++] = ble->event_type; // Event Press
    //Device name
    packet[i++] = ble->device_name_len + 1; // Lenght
    packet[i++] = 0x09; // Full name

    for(size_t j = 0; j < ble->device_name_len; j++) {
        packet[i++] = (uint8_t)ble->device_name[j];
    }

    if(i > EXTRA_BEACON_MAX_DATA_SIZE) {
        FURI_LOG_E(BT_TAG, "Packet too big: Max = %u, Size = %u", EXTRA_BEACON_MAX_DATA_SIZE, i);
        free(packet);
        return false;
    }

    packet = realloc(packet, i);
    *_size = i;
    *_packet = packet;

    return true;
}

void randomize_mac(uint8_t address[EXTRA_BEACON_MAC_ADDR_SIZE]) {
    furi_hal_random_fill_buf(address, EXTRA_BEACON_MAC_ADDR_SIZE);
}

void pretty_print_mac(FuriString* mac_str, uint8_t address[EXTRA_BEACON_MAC_ADDR_SIZE]) {
    furi_string_set_str(mac_str, "");
    for(size_t i = 0; i < EXTRA_BEACON_MAC_ADDR_SIZE - 1; i++) {
        furi_string_cat_printf(mac_str, "%02X", address[i]);
        if(i < EXTRA_BEACON_MAC_ADDR_SIZE - 2) {
            furi_string_cat_str(mac_str, ":");
        }
    }
    FURI_LOG_I(BT_TAG, "Current MAC address: %s", furi_string_get_cstr(mac_str));
}

int32_t bt_comm_worker(void* context) {
    BtBeacon* ble = context;
    bool run = true;

    while(run) {
        uint32_t events = furi_thread_flags_wait(
            ThreadCommStop | ThreadCommStopCmd | ThreadCommSendCmd,
            FuriFlagWaitAny,
            FuriWaitForever);
        if(events & ThreadCommStop) {
            run = false;
            FURI_LOG_I(TAG, "Thread event: Stop command request");

        } else if(events & ThreadCommStopCmd) {
            ble->status = BEACON_INACTIVE;
            FURI_LOG_I(BT_TAG, "Resetting Beacon...");
            if(furi_hal_bt_extra_beacon_is_active()) {
                furi_check(furi_hal_bt_extra_beacon_stop());
            }
            FURI_LOG_I(BT_TAG, "Resetting Beacon done.");
        } else if(events & ThreadCommSendCmd) {
            ble->status = BEACON_BUSY;
            FURI_LOG_I(BT_TAG, "Sending BTHome data...");
            if(furi_hal_bt_extra_beacon_is_active()) {
                furi_check(furi_hal_bt_extra_beacon_stop());
            }

            uint8_t size;
            uint8_t* packet;
            GapExtraBeaconConfig* config = &ble->config;

            furi_check(furi_hal_bt_extra_beacon_set_config(config));
            if(make_packet(ble, &size, &packet)) {
                furi_check(furi_hal_bt_extra_beacon_set_data(packet, size));
                furi_check(furi_hal_bt_extra_beacon_start());
                furi_timer_start(ble->timer_reset_beacon, ble->beacon_duration);
                free(packet);
            }
        }
    }
    FURI_LOG_I(TAG, "Thread event: Stopping...");
    return 0;
}
