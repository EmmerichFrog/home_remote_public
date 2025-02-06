#pragma once
#include "app.h"
#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>

void view_ha_enter_callback(void* context);
void view_ha_exit_callback(void* context);
void ha_draw_callback(Canvas* canvas, void* model);
bool ha_input_callback(InputEvent* event, void* context);
int32_t ha_comm_worker(void* context);
