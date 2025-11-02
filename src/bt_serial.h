#pragma once
#include "app.h"
#include "libs/serial_profile.h"

typedef enum {
    BtSerialCmdToggle = 1
} BtSerialCmd;

bool init_bt_serial(App* app);
bool deinit_bt_serial(App* app);
void bt_serial_write(App* app, BtSerialCmd cmd, char entity[3]);
