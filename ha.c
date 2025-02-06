#include "ha.h"

static const char HA_HEADER[] = "{\"Content-Type\": \"application/json\"}";
static const char HA_SENSORS_PAYLOAD[] = "{\"token\": \"%s\"}";
const char HA_CMD_PAYLOAD[] = "{\"token\": \"%s\",\"entity\":\"%s\"}";

static const char HA_BEDROOM_TEMP_KEY[] = "bedroom_temp";
static const char HA_BEDROOM_HUM_KEY[] = "bedroom_hum";
static const char HA_KITCHEN_TEMP_KEY[] = "kitchen_temp";
static const char HA_KITCHEN_HUM_KEY[] = "kitchen_hum";
static const char HA_OUTSIDE_TEMP_KEY[] = "outside_temp";
static const char HA_OUTSIDE_HUM_KEY[] = "outside_hum";
static const char HA_DEHUM_KEY[] = "dehum";
static const char HA_CO2_KEY[] = "co2";
static const char HA_PM2_5_KEY[] = "pm2_5";

const char HA_DEHUM_ENTITY[] = "switch.dehumidifier";

extern FlipperHTTP* fhttp;

/**
 * @brief      Populate the values for the Home Assistant page
 * @param      model the Home Assistant model
 * @return     true if json data was found
*/
static bool populate_vals(ReqModel* model) {
    char* response = get_last_response(fhttp);
    if(response[0] == '{') {
        FURI_LOG_I(TAG, "Parsing json");

        uint16_t max_tokens = 128;
        // Must free the return value memory
        char* value;
        value = get_json_value(HA_BEDROOM_TEMP_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_bedroom_temp, value);
            free(value);
        }
        value = get_json_value(HA_BEDROOM_HUM_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_bedroom_hum, value);
            free(value);
        }

        value = get_json_value(HA_KITCHEN_TEMP_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_kitchen_temp, value);
            free(value);
        }
        value = get_json_value(HA_KITCHEN_HUM_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_kitchen_hum, value);
            free(value);
        }

        value = get_json_value(HA_OUTSIDE_TEMP_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_outside_temp, value);
            free(value);
        }
        value = get_json_value(HA_OUTSIDE_HUM_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_outside_hum, value);
            free(value);
        }

        value = get_json_value(HA_DEHUM_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_dehum_sts, value);
            free(value);
        }

        value = get_json_value(HA_CO2_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_co2, value);
            free(value);
        }

        value = get_json_value(HA_PM2_5_KEY, response, max_tokens);
        if(value) {
            furi_string_set_str(model->print_pm2_5, value);
            free(value);
        }
        return true;
    } else {
        FURI_LOG_I(TAG, "No json in last_response, skipping");
        return false;
    }
}

/**
 * @brief      Callback of the timer_draw to update the canvas.
 * @details    This function is called when the timer_draw ticks. Also update the data
 * @param      context  The context - App object.
*/
static void view_ha_timer_callback(void* context) {
    App* app = (App*)context;
    ReqModel* ha_model = view_get_model(app->view_ha);

    // If the mutex is not available the canvas is not finished drawing, so skip this timer tick.
    // Callback function cannot block so waiting is not allowed
    if(furi_mutex_acquire(ha_model->worker_mutex, 0) == FuriStatusOk) {
        // If mutex is available it's not needed here so release it
        furi_check(furi_mutex_release(ha_model->worker_mutex) == FuriStatusOk);

        view_dispatcher_send_custom_event(app->view_dispatcher, EventIdHaRedrawScreen);
    }
}

/**
 * @brief      Callback of the ha screen on enter.
 * @details    Prepare the timer_draw and reset get status.
 * @param      context  The context - App object.
*/
void view_ha_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(10U);
    App* app = (App*)context;
    app->current_view = ViewHa;
    ReqModel* ha_model = view_get_model(app->view_ha);

    char buffer[sizeof(HA_SENSORS_PAYLOAD) + ha_model->token_lenght];
    snprintf(
        buffer,
        sizeof(HA_SENSORS_PAYLOAD) + ha_model->token_lenght,
        HA_SENSORS_PAYLOAD,
        furi_string_get_cstr(ha_model->token));
    furi_string_set_str(ha_model->payload, buffer);

    char buffer2[sizeof(HA_CMD_PAYLOAD) + ha_model->token_lenght + sizeof(HA_DEHUM_ENTITY)];
    snprintf(
        buffer2,
        sizeof(HA_CMD_PAYLOAD) + ha_model->token_lenght + sizeof(HA_DEHUM_ENTITY),
        HA_CMD_PAYLOAD,
        furi_string_get_cstr(ha_model->token),
        HA_DEHUM_ENTITY);
    furi_string_set_str(ha_model->payload_dehum, buffer2);

    if(!flipper_http_save_wifi(
           fhttp, furi_string_get_cstr(app->ha_ssid), furi_string_get_cstr(app->ha_pass))) {
        FURI_LOG_E(TAG, "Failed to connect to Home Assistant WiFi");
    } else {
        FURI_LOG_I(
            TAG,
            "Attempting connection to ssid: %s, pass: %s",
            furi_string_get_cstr(app->ha_ssid),
            furi_string_get_cstr(app->ha_pass));
    }

    app->timer_draw = furi_timer_alloc(view_ha_timer_callback, FuriTimerTypePeriodic, context);

    // This timer reset the pressed key graphics
    app->timer_reset_key =
        furi_timer_alloc(view_timer_key_reset_callback, FuriTimerTypeOnce, context);

    ha_model->req_sts = false;
    ha_model->populated = false;

    furi_timer_start(app->timer_draw, period);

    furi_thread_set_callback(app->comm_thread, ha_comm_worker);
    app->timer_comm_upd =
        furi_timer_alloc(comm_thread_timer_callback, FuriTimerTypePeriodic, context);

    furi_timer_start(app->timer_comm_upd, furi_ms_to_ticks(ha_model->polling_rate));

    furi_thread_start(app->comm_thread);
    FURI_LOG_I(TAG, "Comm. thread started with period [%u]ms", ha_model->polling_rate);
    app->comm_thread_id = furi_thread_get_id(app->comm_thread);
    // Update one time on enter
    furi_thread_flags_set(app->comm_thread_id, ThreadCommUpdData);
}

/**
 * @brief      Callback of the ha screen on exit.
 * @details    Stop and free the timer and prepare the text box for the response
 * @param      context  The context - App object.
*/
void view_ha_exit_callback(void* context) {
    App* app = (App*)context;
    furi_timer_flush();
    furi_timer_stop(app->timer_draw);
    furi_timer_free(app->timer_draw);
    app->timer_draw = NULL;
    furi_timer_stop(app->timer_reset_key);
    furi_timer_free(app->timer_reset_key);
    app->timer_reset_key = NULL;
    furi_timer_stop(app->timer_comm_upd);
    furi_timer_free(app->timer_comm_upd);
    app->timer_comm_upd = NULL;

    // Prepare textbox
    text_box_format_msg(app, get_last_response(fhttp), app->text_box_resp);

    // Stop thread and wait for exit
    furi_thread_flags_set(app->comm_thread_id, ThreadCommStop);
    furi_thread_join(app->comm_thread);
}

/**
 * @brief      Callback for drawing the ha view.
 * @details    This function is called when the screen needs to be redrawn.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
void ha_draw_callback(Canvas* canvas, void* model) {
    ReqModel* ha_model = (ReqModel*)model;
    uint8_t http_state = fhttp->state;
    uint8_t resp_state = fhttp->curr_req_sts;
    //bool req_sts = ha_model->req_sts;
    // This mutex will stop the timer to run the function again, in case it's still not finished

    furi_check(furi_mutex_acquire(ha_model->worker_mutex, FuriWaitForever) == FuriStatusOk);

    uint8_t sts = 0;
    if(http_state != IDLE) {
        sts = 2;
    } else if(http_state == IDLE) {
        sts = 1;
    }

    canvas_set_bitmap_mode(canvas, true);
    char page_num[6];
    snprintf(page_num, sizeof(page_num), "%i", ha_model->curr_page);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 116, 8, page_num);
    canvas_set_font(canvas, FontSecondary);
    if(sts == 2) {
        canvas_draw_str(canvas, 75, 7, "Loading");
    }

    switch(ha_model->curr_page) {
    case HaPageFirst:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 8, "Bedroom");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_line(canvas, 0, 9, 126, 9);

        canvas_draw_icon(canvas, 123, 2, &I_ButtonRightSmall_3x5);

        canvas_draw_icon(canvas, 0, 11, &I_weather_temperature);
        canvas_draw_icon(canvas, 20, 12, &I_rounded_box);
        canvas_draw_icon(canvas, 76, 11, &I_weather_humidity);
        canvas_draw_icon(canvas, 90, 12, &I_rounded_box);

        canvas_draw_str(canvas, 24, 22, furi_string_get_cstr(ha_model->print_bedroom_temp));
        canvas_draw_str(canvas, 96, 22, furi_string_get_cstr(ha_model->print_bedroom_hum));

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 40, "Kitchen");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_line(canvas, 0, 43, 126, 43);

        canvas_draw_icon(canvas, 0, 45, &I_weather_temperature);
        canvas_draw_icon(canvas, 20, 46, &I_rounded_box);
        canvas_draw_icon(canvas, 76, 44, &I_weather_humidity);
        canvas_draw_icon(canvas, 90, 46, &I_rounded_box);

        canvas_draw_str(canvas, 24, 56, furi_string_get_cstr(ha_model->print_kitchen_temp));
        canvas_draw_str(canvas, 96, 56, furi_string_get_cstr(ha_model->print_kitchen_hum));

        break;

    case HaPageSecond:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 8, "Outside");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_line(canvas, 0, 9, 126, 9);

        canvas_draw_icon(canvas, 111, 2, &I_ButtonLeftSmall_3x5);
        canvas_draw_icon(canvas, 123, 2, &I_ButtonRightSmall_3x5);

        canvas_draw_icon(canvas, 0, 11, &I_weather_temperature);
        canvas_draw_icon(canvas, 20, 12, &I_rounded_box);
        canvas_draw_icon(canvas, 76, 11, &I_weather_humidity);
        canvas_draw_icon(canvas, 90, 12, &I_rounded_box);

        canvas_draw_str(canvas, 24, 22, furi_string_get_cstr(ha_model->print_outside_temp));
        canvas_draw_str(canvas, 96, 22, furi_string_get_cstr(ha_model->print_outside_hum));

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 40, "Dehum.");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_line(canvas, 0, 43, 126, 43);

        canvas_draw_icon(canvas, 2, 51, &I_power_text_24x5);
        canvas_draw_icon(canvas, 32, 46, &I_rounded_box);

        canvas_draw_str(canvas, 45, 55, furi_string_get_cstr(ha_model->print_dehum_sts));

        canvas_draw_icon(canvas, 94, 58, &I_InfraredArrowDown_4x8);

        if(ha_model->last_input == InputKeyDown) {
            canvas_draw_icon(canvas, 105, 45, &I_power_hover_19x20);
        } else {
            canvas_draw_icon(canvas, 105, 45, &I_power_19x20);
        }

        break;

    case HaPageLast:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 8, "Air");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_line(canvas, 0, 9, 126, 9);

        canvas_draw_icon(canvas, 111, 2, &I_ButtonLeftSmall_3x5);

        canvas_draw_str(canvas, 0, 22, "CO2");
        canvas_draw_icon(canvas, 20, 12, &I_rounded_box);
        canvas_draw_str(canvas, 57, 22, "PM 2.5");
        canvas_draw_icon(canvas, 90, 12, &I_rounded_box);

        canvas_draw_str(canvas, 24, 22, furi_string_get_cstr(ha_model->print_co2));
        canvas_draw_str(canvas, 96, 22, furi_string_get_cstr(ha_model->print_pm2_5));

        break;
    default:
        break;
    }

    if(resp_state == PROCESSING_DONE && http_state == IDLE && !ha_model->populated) {
        ha_model->populated = populate_vals(ha_model);
    }

    furi_check(furi_mutex_release(ha_model->worker_mutex) == FuriStatusOk);
}

/**
 * @brief      Callback for ha screen input.
 * @details    This function is called when the user presses a button while on the ha screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - App object.
 * @return     true if the event was handled, false otherwise.
*/
bool ha_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;
    ReqModel* ha_model = view_get_model(app->view_ha);
    // Update the timer used for resetting the button presses on screen
    if(event->key == InputKeyDown) {
        if(ha_model->last_input == event->key || ha_model->last_input == INPUT_RESET) {
            uint32_t period = furi_ms_to_ticks(200);
            furi_timer_restart(app->timer_reset_key, period);
        } else {
            furi_timer_stop(app->timer_reset_key);
            ha_model->last_input = INPUT_RESET;
        }
    }
    // Status used for drawing button presses
    ha_model->last_input = event->key;
    int8_t index;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft:
            index = ha_model->curr_page - 1;
            if(index >= HaPageFirst) {
                ha_model->curr_page = index;
            }
            break;
        case InputKeyRight:
            index = ha_model->curr_page + 1;
            if(index <= HaPageLast) {
                ha_model->curr_page = index;
            }
            break;
        case InputKeyDown:
            if(ha_model->curr_page == HaPageSecond) {
                furi_thread_flags_set(app->comm_thread_id, ThreadCommSendCmd);
            }
            break;
        case InputKeyBack:
            view_dispatcher_send_custom_event(app->view_dispatcher, EventIdHaCheckBack);
            break;
        default:
            return false;
        }
        return true;
    }
    return false;
}

int32_t ha_comm_worker(void* context) {
    App* app = (App*)context;
    ReqModel* ha_model = view_get_model(app->view_ha);
    bool run = true;

    while(run) {
        uint32_t events = furi_thread_flags_wait(
            ThreadCommStop | ThreadCommUpdData | ThreadCommSendCmd,
            FuriFlagWaitAny,
            FuriWaitForever);
        if(events & ThreadCommStop) {
            run = false;
            FURI_LOG_I(TAG, "Thread event: Stop command request");

        } else if(events & ThreadCommSendCmd) {
            FURI_LOG_I(TAG, "Thread event: Send command");
            while(!allow_cmd(ha_model)) {
                FURI_LOG_I(TAG, "Thread event: Send command Esp32 busy, waiting...");
                furi_delay_ms(100);
            }
            FURI_LOG_I(TAG, "Thread event: Sending command...");
            ha_model->req_sts = flipper_http_post_request_with_headers(
                fhttp,
                furi_string_get_cstr(ha_model->url_cmd),
                HA_HEADER,
                furi_string_get_cstr(ha_model->payload_dehum));
            if(ha_model->req_sts) {
                FURI_LOG_I(TAG, "Thread event: Command sent");
            } else {
                FURI_LOG_E(TAG, "Thread event: Command send failed");
            }
        } else if(events & ThreadCommUpdData) {
            if(allow_cmd(ha_model)) {
                FURI_LOG_I(TAG, "Thread event: Sending update req...");
                ha_model->req_sts = flipper_http_post_request_with_headers(
                    fhttp,
                    furi_string_get_cstr(ha_model->url),
                    HA_HEADER,
                    furi_string_get_cstr(ha_model->payload));
                if(ha_model->req_sts) {
                    FURI_LOG_I(TAG, "Thread event: Update req. sent");
                    ha_model->populated = false;
                } else {
                    FURI_LOG_E(TAG, "Thread event: Update req. failed");
                }
            }
        }
    }
    FURI_LOG_I(TAG, "Thread event: Stopping...");
    return 0;
}
