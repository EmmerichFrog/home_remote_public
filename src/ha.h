#pragma once
#include "app.h"
#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>

#define SGHZ_DEFAULT_STR "00bt0000bh0000kt0000kh0000ot0000oh0000dhxoffadxoffco0000pm0000"

void ha_enter_callback(void* context);
void ha_exit_callback(void* context);
void ha_draw_callback(Canvas* canvas, void* model);
bool ha_input_callback(InputEvent* event, void* context);
int32_t ha_http_worker(void* context);
