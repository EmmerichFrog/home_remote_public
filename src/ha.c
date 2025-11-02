#include "ha.h"
#include "ha_helpers.h"
#include "ble_beacon.h"
#include "sghz.h"
#include "src/bt_serial.h"

static const char HA_HEADER[] = "{\"Content-Type\": \"application/json\"}";
static const char HA_SENSORS_PAYLOAD[] = "{\"token\": \"%s\"}";
const char HA_CMD_PAYLOAD[] = "{\"token\": \"%s\",\"entity\":\"%s\"}";

const char HA_DEHUM_ENTITY[] = "switch.dehumidifier";

extern FlipperHTTP* fhttp;

/**
 * @brief      Populate the values for the Home Assistant page
 * @param      model the Home Assistant model
 * @return     true if json data was found
*/
static bool populate_vals(ReqModel* ha_model) {
    switch(ha_model->control_mode) {
    case HaCtrlWifi:
        const char* response = get_last_response(fhttp);
        if(response[0] == '{') {
            FURI_LOG_I(TAG, "Parsing json");
            parse_ha_json(response, ha_model);
            return true;
        } else {
            FURI_LOG_I(TAG, "No json in last_response, skipping");
        }
        break;

    case HaCtrlSghzBtHome:
        parse_ha_sghz(furi_string_get_cstr(ha_model->sghz->last_message), ha_model);
        return true;

    case HaCtrlBtSerial:
        parse_ha_bt_serial(&ha_model->bt_serial->data, ha_model);
        return true;

    default:
        FURI_LOG_E(TAG, "Not implemented");
        break;
    }

    return false;
}

/**
 * @brief      Callback of the timer_draw to update the canvas.
 * @details    This function is called when the timer_draw ticks. Also update the data
 * @param      context  The context - App object.
*/
static void view_ha_timer_callback(void* context) {
    App* app = (App*)context;
    ReqModel* ha_model = view_get_model(app->view_ha);

    // If the mutex is not available the canvas has not finished drawing, so skip this timer tick.
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
void ha_enter_callback(void* context) {
    App* app = context;
    app->current_view = ViewHa;
    ReqModel* ha_model = view_get_model(app->view_ha);

    switch(ha_model->control_mode) {
    case HaCtrlWifi: {
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
        app->comm_thread = furi_thread_alloc_ex("Comm_Thread", 2048, ha_http_worker, app);
        furi_thread_start(app->comm_thread);
        FURI_LOG_I(TAG, "Comm. thread started with period [%u]ms", ha_model->polling_rate);
        app->comm_thread_id = furi_thread_get_id(app->comm_thread);
        // Update one time on enter
        furi_thread_flags_set(app->comm_thread_id, ThreadCommUpdData);
        app->timer_comm_upd =
            furi_timer_alloc(comm_thread_timer_callback, FuriTimerTypePeriodic, context);
        furi_timer_start(app->timer_comm_upd, furi_ms_to_ticks(ha_model->polling_rate));
        ha_model->req_sts = false;
    } break;

    case HaCtrlSghzBtHome:
        ha_init_ble(app);
        // Subghz Setup
        ha_model->sghz->status = SGHZ_INIT;
        ha_model->sghz->last_message = furi_string_alloc();
        furi_string_set_str(ha_model->sghz->last_message, SGHZ_DEFAULT_STR);
        furi_hal_power_suppress_charge_enter();
        subghz_devices_init();
        uint32_t frequency = 433920000;
        ha_model->sghz->subghz_txrx = subghz_tx_rx_worker_alloc();
        ha_model->sghz->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);

        if(subghz_tx_rx_worker_start(
               ha_model->sghz->subghz_txrx, ha_model->sghz->device, frequency)) {
            subghz_tx_rx_worker_set_callback_have_read(
                ha_model->sghz->subghz_txrx, subghz_worker_update_rx, app);
        }
        FURI_LOG_I(TAG, "Listening at frequency: %lu\r\n", frequency);
        ha_model->sghz->rx_thread = furi_thread_alloc_ex("rx_sghz", 1024, listen_rx, ha_model);
        furi_thread_start(ha_model->sghz->rx_thread);
        ha_model->sghz->rx_thread_id = furi_thread_get_id(ha_model->sghz->rx_thread);
        // Subghz Setup End
        break;

    case HaCtrlBtSerial:
        init_bt_serial(app);
        break;

    default:
        FURI_LOG_E(TAG, "Not implemented");
        break;
    }

    app->timer_draw = furi_timer_alloc(view_ha_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer_draw, furi_ms_to_ticks(DRAW_PERIOD));

    // This timer reset the pressed key graphics
    app->timer_reset_key =
        furi_timer_alloc(view_timer_key_reset_callback, FuriTimerTypeOnce, context);

    ha_model->populated = false;
}

/**
 * @brief      Callback of the ha screen on exit.
 * @details    Stop and free the timer and prepare the text box for the response
 * @param      context  The context - App object.
*/
void ha_exit_callback(void* context) {
    App* app = (App*)context;
    ReqModel* ha_model = view_get_model(app->view_ha);

    furi_timer_flush();
    furi_timer_stop(app->timer_draw);
    furi_timer_free(app->timer_draw);
    app->timer_draw = NULL;
    furi_timer_stop(app->timer_reset_key);
    furi_timer_free(app->timer_reset_key);
    app->timer_reset_key = NULL;

    switch(ha_model->control_mode) {
    case HaCtrlWifi:
        furi_timer_stop(app->timer_comm_upd);
        furi_timer_free(app->timer_comm_upd);
        app->timer_comm_upd = NULL;
        // Prepare textbox
        futils_text_box_format_msg(
            app->formatted_message, get_last_response(fhttp), app->text_box_resp);

        // Stop thread and wait for exit
        if(app->comm_thread) {
            furi_thread_flags_set(app->comm_thread_id, ThreadCommStop);
            furi_thread_join(app->comm_thread);
            furi_thread_free(app->comm_thread);
        }
        break;

    case HaCtrlSghzBtHome:
        ha_deinit_ble(app);
        // Subghz Cleanup
        // Stop thread and wait for exit
        if(ha_model->sghz->rx_thread) {
            furi_thread_flags_set(ha_model->sghz->rx_thread_id, ThreadCommStop);
            furi_thread_join(ha_model->sghz->rx_thread);
            furi_thread_free(ha_model->sghz->rx_thread);
        }

        // Shutdown radio
        subghz_devices_sleep(ha_model->sghz->device);
        subghz_devices_end(ha_model->sghz->device);
        subghz_devices_deinit();

        // Cleanup
        furi_string_free(ha_model->sghz->last_message);
        if(subghz_tx_rx_worker_is_running(ha_model->sghz->subghz_txrx)) {
            subghz_tx_rx_worker_stop(ha_model->sghz->subghz_txrx);
        }

        subghz_tx_rx_worker_free(ha_model->sghz->subghz_txrx);

        furi_hal_power_suppress_charge_exit();
        // Subghz Cleanup End
        break;

    case HaCtrlBtSerial:
        deinit_bt_serial(app);
        break;

    default:
        FURI_LOG_E(TAG, "Not implemented");
        break;
    }
}

/**
 * @brief      Callback for drawing the ha view.
 * @details    This function is called when the screen needs to be redrawn.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
void ha_draw_callback(Canvas* canvas, void* model) {
    ReqModel* ha_model = (ReqModel*)model;
    const uint8_t http_state = fhttp->state;
    const uint8_t resp_state = fhttp->curr_req_sts;
    //bool req_sts = ha_model->req_sts;
    // This mutex will stop the timer to run the function again, in case it's still not finished
    if(furi_mutex_acquire(ha_model->worker_mutex, 0) == FuriStatusOk) {
        if((ha_model->control_mode == HaCtrlWifi && resp_state == PROCESSING_DONE &&
            http_state == IDLE && !ha_model->populated) ||
           (ha_model->control_mode == HaCtrlSghzBtHome &&
            ha_model->sghz->status == SGHZ_INACTIVE) ||
           (ha_model->control_mode == HaCtrlBtSerial && true)) {
            ha_model->populated = populate_vals(ha_model);
        }
        canvas_set_bitmap_mode(canvas, true);

        if((ha_model->control_mode == HaCtrlWifi &&
            (http_state != IDLE || resp_state == PROCESSING_BUSY)) ||
           (ha_model->control_mode == HaCtrlSghzBtHome && ha_model->sghz->status == SGHZ_BUSY)) {
            canvas_draw_str(canvas, 75, 7, "Loading");
        }

        switch(ha_model->curr_page) {
        case PageFirst:
            futils_draw_header(canvas, "Bedroom", ha_model->curr_page, 8);
            canvas_draw_icon(canvas, 123, 2, &I_ButtonRightSmall_3x5);

            canvas_draw_icon(canvas, 0, 11, &I_weather_temperature);
            canvas_draw_icon(canvas, 20, 12, &I_rounded_box);
            canvas_draw_icon(canvas, 76, 11, &I_weather_humidity);
            canvas_draw_icon(canvas, 90, 12, &I_rounded_box);

            canvas_draw_str(canvas, 26, 22, furi_string_get_cstr(ha_model->print_bedroom_temp));
            canvas_draw_str(canvas, 96, 22, furi_string_get_cstr(ha_model->print_bedroom_hum));

            futils_draw_header(canvas, "Kitchen", NO_PAGE_NUM, 40);

            canvas_draw_icon(canvas, 0, 45, &I_weather_temperature);
            canvas_draw_icon(canvas, 20, 46, &I_rounded_box);
            canvas_draw_icon(canvas, 76, 44, &I_weather_humidity);
            canvas_draw_icon(canvas, 90, 46, &I_rounded_box);

            canvas_draw_str(canvas, 26, 56, furi_string_get_cstr(ha_model->print_kitchen_temp));
            canvas_draw_str(canvas, 96, 56, furi_string_get_cstr(ha_model->print_kitchen_hum));

            break;

        case PageSecond:
            futils_draw_header(canvas, "Outside", ha_model->curr_page, 8);
            canvas_draw_icon(canvas, 111, 2, &I_ButtonLeftSmall_3x5);
            canvas_draw_icon(canvas, 123, 2, &I_ButtonRightSmall_3x5);

            canvas_draw_icon(canvas, 0, 11, &I_weather_temperature);
            canvas_draw_icon(canvas, 20, 12, &I_rounded_box);
            canvas_draw_icon(canvas, 76, 11, &I_weather_humidity);
            canvas_draw_icon(canvas, 90, 12, &I_rounded_box);

            canvas_draw_str(canvas, 26, 22, furi_string_get_cstr(ha_model->print_outside_temp));
            canvas_draw_str(canvas, 96, 22, furi_string_get_cstr(ha_model->print_outside_hum));

            futils_draw_header(canvas, "Dehum.", NO_PAGE_NUM, 40);

            canvas_draw_icon(canvas, 2, 51, &I_power_text_24x5);
            canvas_draw_icon(canvas, 32, 46, &I_rounded_box);

            canvas_draw_str(canvas, 38, 56, furi_string_get_cstr(ha_model->print_dehum_sts));

            canvas_draw_icon(canvas, 94, 58, &I_InfraredArrowDown_4x8);

            if(ha_model->last_input == InputKeyDown) {
                canvas_draw_icon(canvas, 105, 45, &I_power_hover_19x20);
            } else {
                canvas_draw_icon(canvas, 105, 45, &I_power_19x20);
            }

            break;

        case PageThird:
            futils_draw_header(canvas, "Air", ha_model->curr_page, 8);

            canvas_draw_icon(canvas, 111, 2, &I_ButtonLeftSmall_3x5);

            canvas_draw_str(canvas, 0, 22, "CO2");
            canvas_draw_icon(canvas, 20, 12, &I_rounded_box);
            canvas_draw_str(canvas, 57, 22, "PM 2.5");
            canvas_draw_icon(canvas, 90, 12, &I_rounded_box);

            canvas_draw_str(canvas, 26, 22, furi_string_get_cstr(ha_model->print_co2));
            canvas_draw_str(canvas, 96, 22, furi_string_get_cstr(ha_model->print_pm2_5));

            break;
        default:
            break;
        }

        furi_check(furi_mutex_release(ha_model->worker_mutex) == FuriStatusOk);
    }
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
            furi_timer_restart(app->timer_reset_key, furi_ms_to_ticks(RESET_KEY_PERIOD));
        } else {
            furi_timer_stop(app->timer_reset_key);
            ha_model->last_input = INPUT_RESET;
        }
    }
    // Status used for drawing button presses
    ha_model->last_input = event->key;
    int8_t p_index;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft:
            p_index = ha_model->curr_page - 1;
            if(p_index >= PageFirst) {
                ha_model->curr_page = p_index;
            }
            break;
        case InputKeyRight:
            p_index = ha_model->curr_page + 1;
            if(p_index < PageLast) {
                ha_model->curr_page = p_index;
            }
            break;
        case InputKeyDown:
            if(ha_model->curr_page == PageSecond) {
                if(ha_model->control_mode == HaCtrlWifi) {
                    furi_thread_flags_set(app->comm_thread_id, ThreadCommSendCmd);
                } else if(ha_model->control_mode == HaCtrlSghzBtHome) {
                    ha_model->ble->event_type = BTHomeShortPress;
                    furi_thread_flags_set(app->comm_thread_id, ThreadCommSendCmd);
                } else if(ha_model->control_mode == HaCtrlBtSerial) {
                    char entity[3] = "dh";
                    bt_serial_write(app, BtSerialCmdToggle, entity);
                }
            }
            break;
        case InputKeyBack:
            view_dispatcher_send_custom_event(app->view_dispatcher, EventIdHaCheckBack);
            break;
        case InputKeyOk:
            ha_model->populated = populate_vals(ha_model);
            break;
        default:
            return false;
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, EventIdHaRedrawScreen);
        return true;
    } else if(event->type == InputTypeLong) {
        switch(event->key) {
        case InputKeyDown:
            if(ha_model->curr_page == PageSecond) {
                if(ha_model->control_mode == HaCtrlWifi) {
                    furi_thread_flags_set(app->comm_thread_id, ThreadCommSendCmd);
                } else if(ha_model->control_mode == HaCtrlSghzBtHome) {
                    ha_model->ble->event_type = BTHomeLongPress;
                    furi_thread_flags_set(app->comm_thread_id, ThreadCommSendCmd);
                } else if(ha_model->control_mode == HaCtrlBtSerial) {
                    char entity[3] = "ad";
                    bt_serial_write(app, BtSerialCmdToggle, entity);
                }
            }

            break;
        case InputKeyBack:
            view_dispatcher_send_custom_event(app->view_dispatcher, EventIdHaCheckBack);
            break;
        default:
            return false;
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, EventIdHaRedrawScreen);
        return true;
    }
    return false;
}

int32_t ha_http_worker(void* context) {
    App* app = context;
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
            notification_message(app->notification, &sequence_blink_green_100);
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
                fhttp->curr_req_sts = PROCESSING_BUSY;
                FURI_LOG_I(TAG, "Thread event: Sending update req...");
                notification_message(app->notification, &sequence_blink_blue_100);
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
