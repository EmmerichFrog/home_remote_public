#pragma once
#include "app.h"

#define BEACON_PERIOD   20U
#define BEACON_DURATION 1000U

#define MAX_NAME_LENGHT 15

App* app_alloc();
void app_free(App* app);
