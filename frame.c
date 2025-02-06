#include "frame.h"

static const char FRAME_PREV_PATH[] = "/prev";
static const char FRAME_NEXT_PATH[] = "/next";
static const char FRAME_RAND_PATH[] = "/rand";
static const char FRAME_SHUTDOWN_PATH[] = "/shutdown";

extern FlipperHTTP* fhttp;

/**
 * @brief      Callback of the timer_draw to update the canvas.
 * @details    This function is called when the timer_draw ticks.
 * @param      context  The context - App object.
*/
static void view_frame_timer_callback(void* context) {
    App* app = (App*)context;
    ReqModel* frame_model = view_get_model(app->view_frame);
    // If the mutex is not available the canvas is not finished drawing, so skip this timer tick.
    // Callback function cannot block so waiting is not allowed
    if(furi_mutex_acquire(frame_model->worker_mutex, 0) == FuriStatusOk) {
        // If mutex is available it's not needed here so release it
        furi_check(furi_mutex_release(frame_model->worker_mutex) == FuriStatusOk);
        view_dispatcher_send_custom_event(app->view_dispatcher, EventIdFrameRedrawScreen);
    }
}

/**
 * @brief      Callback of the frame screen on enter.
 * @details    Prepare the timer_draw and reset get status.
 * @param      context  The context - App object.
*/
void view_frame_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(10U);
    App* app = (App*)context;
    app->current_view = ViewFrame;
    ReqModel* frame_model = view_get_model(app->view_frame);

    if(!flipper_http_save_wifi(
           fhttp, furi_string_get_cstr(app->frame_ssid), furi_string_get_cstr(app->frame_pass))) {
        FURI_LOG_E(TAG, "Failed to connect to frame WiFi");
    } else {
        FURI_LOG_I(
            TAG,
            "Attempting connection to ssid: %s, pass: %s",
            furi_string_get_cstr(app->frame_ssid),
            furi_string_get_cstr(app->frame_pass));
    }

    app->timer_draw = furi_timer_alloc(view_frame_timer_callback, FuriTimerTypePeriodic, context);
    // This timer reset the pressed key graphics
    app->timer_reset_key =
        furi_timer_alloc(view_timer_key_reset_callback, FuriTimerTypeOnce, context);
    frame_model->req_sts = false;
    furi_timer_start(app->timer_draw, period);
}

/**
 * @brief      Callback of the frame screen on exit.
 * @details    Stop and free the timer and prepare the text box for the response
 * @param      context  The context - App object.
*/
void view_frame_exit_callback(void* context) {
    App* app = (App*)context;
    furi_timer_flush();
    furi_timer_stop(app->timer_draw);
    furi_timer_free(app->timer_draw);
    app->timer_draw = NULL;
    furi_timer_stop(app->timer_reset_key);
    furi_timer_free(app->timer_reset_key);
    app->timer_reset_key = NULL;

    // Prepare textbox
    text_box_format_msg(app, get_last_response(fhttp), app->text_box_resp);
}

/**
 * @brief      Callback for drawing the frame view.
 * @details    This function is called when the screen needs to be redrawn.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
void frame_draw_callback(Canvas* canvas, void* model) {
    ReqModel* frame_model = (ReqModel*)model;
    uint8_t dolphin_sts = 0;
    uint8_t http_state = fhttp->state;
    uint8_t resp_state = fhttp->curr_req_sts;

    FuriString* cmd = furi_string_alloc();
    furi_string_set_str(cmd, furi_string_get_cstr(frame_model->curr_cmd));
    bool req_sts = frame_model->req_sts;
    // This mutex will stop the timer to run the function again, in case it's still not finished
    furi_check(furi_mutex_acquire(frame_model->worker_mutex, FuriWaitForever) == FuriStatusOk);

    canvas_set_bitmap_mode(canvas, true);
    // Draw pressed or not grahic
    if(frame_model->last_input == InputKeyDown) {
        canvas_draw_icon(canvas, 87, 31, &I_down_hover);
    } else {
        canvas_draw_icon(canvas, 87, 31, &I_down);
    }

    if(frame_model->last_input == InputKeyLeft) {
        canvas_draw_icon(canvas, 67, 10, &I_left_hover);
    } else {
        canvas_draw_icon(canvas, 67, 10, &I_left);
    }

    if(frame_model->last_input == InputKeyOk) {
        canvas_draw_icon(canvas, 87, 10, &I_ok_hover);
    } else {
        canvas_draw_icon(canvas, 87, 10, &I_ok);
    }

    if(frame_model->last_input == InputKeyRight) {
        canvas_draw_icon(canvas, 107, 10, &I_right_hover);
    } else {
        canvas_draw_icon(canvas, 107, 10, &I_right);
    }

    if(!frame_model->last_input == InputKeyUp) {
        canvas_draw_icon(canvas, 109, 59, &I_Pin_pointer_5x3);
    }

    canvas_draw_icon(canvas, 107, 4, &I_next_text_19x6);
    canvas_draw_icon(canvas, 67, 4, &I_prev_text_19x5);
    canvas_draw_icon(canvas, 93, 1, &I_BLE_beacon_7x8);
    canvas_draw_icon(canvas, 90, 52, &I_shuffle);
    canvas_draw_icon(canvas, 115, 58, &I_off_text_12x5);

    if(http_state == ISSUE && req_sts) {
        dolphin_sts = 0;
    } else if((resp_state == PROCESSING_BUSY || req_sts) && resp_state != PROCESSING_DONE) {
        dolphin_sts = 2;
    } else if(resp_state == PROCESSING_DONE || (!req_sts && http_state == IDLE)) {
        dolphin_sts = 1;
    }

    switch(dolphin_sts) {
    case 0:
        canvas_draw_icon(canvas, 0, 16, &I_dolph_cry_49x54);
        canvas_draw_icon(canvas, 50, 35, &I_box);
        canvas_draw_str(canvas, 52, 44, furi_string_get_cstr(cmd));
        break;
    case 1:
        canvas_draw_icon(canvas, -1, 16, &I_DolphinCommon);
        canvas_draw_icon(canvas, 38, 21, &I_box);
        canvas_draw_str(canvas, 40, 30, furi_string_get_cstr(cmd));
        break;
    case 2:
        canvas_draw_icon(canvas, 0, 9, &I_NFC_dolphin_emulation_51x64);
        break;
    default:
        break;
    }
    furi_check(furi_mutex_release(frame_model->worker_mutex) == FuriStatusOk);
    furi_string_free(cmd);
}

/**
 * @brief      Callback for frame screen input.
 * @details    This function is called when the user presses a button while on the frame screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - App object.
 * @return     true if the event was handled, false otherwise.
*/
bool frame_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;
    ReqModel* frame_model = view_get_model(app->view_frame);
    // Update the timer used for resetting the button presses on screen
    if(event->key == InputKeyOk || event->key == InputKeyLeft || event->key == InputKeyRight ||
       event->key == InputKeyUp || event->key == InputKeyDown) {
        if(frame_model->last_input == event->key || frame_model->last_input == INPUT_RESET) {
            uint32_t period = furi_ms_to_ticks(200);
            furi_timer_restart(app->timer_reset_key, period);
        } else {
            furi_timer_stop(app->timer_reset_key);
            frame_model->last_input = INPUT_RESET;
        }
    }
    // Status used for drawing button presses
    frame_model->last_input = event->key;

    // Ok sends the command selected with left, right or down. Up long press sends a shfhttp->utdown command
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyOk:
            if(allow_cmd(frame_model)) {
                frame_model->req_sts =
                    flipper_http_get_request(fhttp, furi_string_get_cstr(frame_model->req_path));
            }
            break;
        case InputKeyLeft:
            if(allow_cmd(frame_model)) {
                furi_string_set_str(frame_model->req_path, furi_string_get_cstr(frame_model->url));
                furi_string_cat_str(frame_model->req_path, FRAME_PREV_PATH);
                furi_string_set_str(frame_model->curr_cmd, "PREV");
            }
            break;
        case InputKeyRight:
            if(allow_cmd(frame_model)) {
                furi_string_set_str(frame_model->req_path, furi_string_get_cstr(frame_model->url));
                furi_string_cat_str(frame_model->req_path, FRAME_NEXT_PATH);
                furi_string_set_str(frame_model->curr_cmd, "NEXT");
            }
            break;
        case InputKeyDown:
            if(allow_cmd(frame_model)) {
                furi_string_set_str(frame_model->req_path, furi_string_get_cstr(frame_model->url));
                furi_string_cat_str(frame_model->req_path, FRAME_RAND_PATH);
                furi_string_set_str(frame_model->curr_cmd, "RAND");
            }
            break;
        case InputKeyBack:
            view_dispatcher_send_custom_event(app->view_dispatcher, EventIdFrameCheckBack);
            break;
        default:
            return false;
        }
        return true;
    } else if(event->type == InputTypeLong) {
        switch(event->key) {
        case InputKeyUp:
            if(allow_cmd(frame_model)) {
                furi_string_set_str(frame_model->req_path, furi_string_get_cstr(frame_model->url));
                furi_string_cat_str(frame_model->req_path, FRAME_SHUTDOWN_PATH);
                furi_string_set_str(frame_model->curr_cmd, "STDN");
                frame_model->req_sts =
                    flipper_http_get_request(fhttp, furi_string_get_cstr(frame_model->req_path));
            }
            break;
        case InputKeyBack:
            view_dispatcher_send_custom_event(app->view_dispatcher, EventIdForceBack);
            break;
        default:
            break;
        }
    }

    return false;
}
