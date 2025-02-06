#pragma once
#include "app.h"
#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>

void view_frame_enter_callback(void* context);
void view_frame_exit_callback(void* context);
void frame_draw_callback(Canvas* canvas, void* model);
bool frame_input_callback(InputEvent* event, void* context);