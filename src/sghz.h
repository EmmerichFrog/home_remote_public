#pragma once
#include "app.h"
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <devices/cc1101_int/cc1101_int_interconnect.h>

#define SGHZ_TAG         "SGHZ"
#define SGHZ_POLL_PERIOD 5000U

void subghz_worker_update_rx(void* context);
int32_t listen_rx(void* context);
