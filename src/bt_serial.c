#include "src/bt_serial.h"

static uint16_t bt_serial_callback(SerialServiceEvent event, void* ctx) {
    furi_assert(ctx);
    App* app = ctx;
    ReqModel* ha_model = view_get_model(app->view_ha);

    if(event.event == SerialServiceEventTypeDataReceived) {
        FURI_LOG_I(
            TAG,
            "SerialServiceEventTypeDataReceived. Size: %u/%u. Data: %s",
            event.data.size,
            sizeof(DataStruct),
            (char*)event.data.buffer);

        if(event.data.size == sizeof(DataStruct)) {
            memcpy(&ha_model->bt_serial->data, event.data.buffer, sizeof(DataStruct));
            ha_model->bt_serial->bt_state = BtStateRecieving;
            ha_model->bt_serial->last_packet = furi_hal_rtc_get_timestamp();
            notification_message(app->notification, &sequence_blink_blue_10);
        }
    }

    return 0;
}

void bt_serial_write(App* app, BtSerialCmd cmd, char entity[3]) {
    ReqModel* ha_model = view_get_model(app->view_ha);
    entity[2] = '\0';
    char buffer[32];

    switch(cmd) {
    case BtSerialCmdToggle:
        snprintf(buffer, sizeof(buffer), "%s:%s", entity, "toggle");
        size_t len = strlen(buffer);

        notification_message(app->notification, &sequence_blink_green_10);
        ble_profile_serial_tx(ha_model->bt_serial->ble_serial_profile, (uint8_t*)buffer, len);
        break;

    default:
        FURI_LOG_I(TAG, "BtSerialCmd not implemented: %u", cmd);
        break;
    }
}

bool init_bt_serial(App* app) {
    ReqModel* ha_model = view_get_model(app->view_ha);

    bt_disconnect(ha_model->bt_serial->bt);
    bt_keys_storage_set_storage_path(ha_model->bt_serial->bt, APP_DATA_PATH(".bt_serial.keys"));

    BleProfileSerialParams params = {
        .device_name_prefix = "HA Rem",
        .mac_xor = 0x0002,
    };
    ha_model->bt_serial->ble_serial_profile =
        bt_profile_start(ha_model->bt_serial->bt, ble_profile_serial, &params);

    furi_check(ha_model->bt_serial->ble_serial_profile);
    ble_profile_serial_set_event_callback(
        ha_model->bt_serial->ble_serial_profile, BT_SERIAL_BUFFER_SIZE, bt_serial_callback, app);
    furi_hal_bt_start_advertising();

    ha_model->bt_serial->bt_state = BtStateWaiting;
    FURI_LOG_D(TAG, "Bluetooth is active!");

    return true;
}

bool deinit_bt_serial(App* app) {
    ReqModel* ha_model = view_get_model(app->view_ha);
    ble_profile_serial_set_event_callback(ha_model->bt_serial->ble_serial_profile, 0, NULL, NULL);
    bt_disconnect(ha_model->bt_serial->bt);
    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(ha_model->bt_serial->bt);
    furi_check(bt_profile_restore_default(ha_model->bt_serial->bt));

    return true;
}
