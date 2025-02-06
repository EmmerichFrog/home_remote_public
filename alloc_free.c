#include "alloc_free.h"
#include "frame.h"
#include "ha.h"

static const char FRAME_PATH_CONFIG_LABEL[] = "Frame URL";
static const char FRAME_SSID_CONFIG_LABEL[] = "Frame SSID";
static const char FRAME_PASS_CONFIG_LABEL[] = "Frame Pass.";

static const char HA_PATH_CONFIG_LABEL[] = "HA Sens. URL";
static const char HA_PATH_CMD_CONFIG_LABEL[] = "HA Cmd URL";
static const char HA_SSID_CONFIG_LABEL[] = "HA SSID";
static const char HA_PASS_CONFIG_LABEL[] = "HA Pass.";
static const char* POLLING_CONFIG_LABEL = "Polling";

extern FlipperHTTP* fhttp;

extern const uint16_t polling_values[4];
extern const char* polling_names[4];

/**
 * @brief      Allocate the application.
 * @details    This function allocates the application resources.
 * @return     App object.
*/
App* app_alloc() {
    App* app = (App*)malloc(sizeof(App));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->config_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Config", SubmenuIndexConfigure, submenu_callback, app);
    submenu_add_item(app->submenu, "Frame Remote", SubmenuIndexFrame, submenu_callback, app);
    submenu_add_item(app->submenu, "Home Assistant", SubmenuIndexHa, submenu_callback, app);
    submenu_add_item(app->submenu, "Response", SubmenuIndexResp, submenu_callback, app);
    submenu_add_item(app->submenu, "About", SubmenuIndexAbout, submenu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), navigation_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);

    //Config
    // Text Inputs
    // Frame Path
    easy_flipper_set_uart_text_input(
        &app->text_input_frame_path,
        ViewTextInputFramePath,
        "Enter URL",
        app->temp_buffer_frame_path,
        app->temp_buffer_frame_path_size,
        conf_text_updated,
        navigation_configure_callback,
        &app->view_dispatcher,
        app);
    app->temp_buffer_frame_path_size = 128;
    app->temp_buffer_frame_path = (char*)malloc(app->temp_buffer_frame_path_size);
    // Pointer in struct assigned later

    // Frame SSID
    easy_flipper_set_uart_text_input(
        &app->text_input_frame_ssid,
        ViewTextInputFrameSsid,
        "Enter SSID",
        app->temp_buffer_frame_ssid,
        app->temp_buffer_frame_ssid_size,
        conf_text_updated,
        navigation_configure_callback,
        &app->view_dispatcher,
        app);
    app->temp_buffer_frame_ssid_size = 64;
    app->temp_buffer_frame_ssid = (char*)malloc(app->temp_buffer_frame_ssid_size);
    app->frame_ssid = furi_string_alloc();

    // Frame Password
    easy_flipper_set_uart_text_input(
        &app->text_input_frame_pass,
        ViewTextInputFramePass,
        "Enter Password",
        app->temp_buffer_frame_pass,
        app->temp_buffer_frame_pass_size,
        conf_text_updated,
        navigation_configure_callback,
        &app->view_dispatcher,
        app);
    app->temp_buffer_frame_pass_size = 64;
    app->temp_buffer_frame_pass = (char*)malloc(app->temp_buffer_frame_pass_size);
    app->frame_pass = furi_string_alloc();

    // Home assistant Path
    easy_flipper_set_uart_text_input(
        &app->text_input_ha_path,
        ViewTextInputHaPath,
        "Enter Sensor Path",
        app->temp_buffer_ha_path,
        app->temp_buffer_ha_path_size,
        conf_text_updated,
        navigation_configure_callback,
        &app->view_dispatcher,
        app);
    app->temp_buffer_ha_path_size = 128;
    app->temp_buffer_ha_path = (char*)malloc(app->temp_buffer_ha_path_size);

    easy_flipper_set_uart_text_input(
        &app->text_input_ha_path_cmd,
        ViewTextInputHaPathCmd,
        "Enter Command Path",
        app->temp_buffer_ha_path_cmd,
        app->temp_buffer_ha_path_cmd_size,
        conf_text_updated,
        navigation_configure_callback,
        &app->view_dispatcher,
        app);
    app->temp_buffer_ha_path_cmd_size = 128;
    app->temp_buffer_ha_path_cmd = (char*)malloc(app->temp_buffer_ha_path_cmd_size);

    // Home assistant SSID
    easy_flipper_set_uart_text_input(
        &app->text_input_ha_ssid,
        ViewTextInputHaSsid,
        "Enter SSID",
        app->temp_buffer_ha_ssid,
        app->temp_buffer_ha_ssid_size,
        conf_text_updated,
        navigation_configure_callback,
        &app->view_dispatcher,
        app);
    app->temp_buffer_ha_ssid_size = 64;
    app->temp_buffer_ha_ssid = (char*)malloc(app->temp_buffer_ha_ssid_size);
    app->ha_ssid = furi_string_alloc();

    // Home assistant Password
    easy_flipper_set_uart_text_input(
        &app->text_input_ha_pass,
        ViewTextInputHaPass,
        "Enter Password",
        app->temp_buffer_ha_pass,
        app->temp_buffer_ha_pass_size,
        conf_text_updated,
        navigation_configure_callback,
        &app->view_dispatcher,
        app);
    app->temp_buffer_ha_pass_size = 64;
    app->temp_buffer_ha_pass = (char*)malloc(app->temp_buffer_ha_pass_size);
    app->ha_pass = furi_string_alloc();

    // Frame View
    app->view_frame = view_alloc();
    view_set_draw_callback(app->view_frame, frame_draw_callback);
    view_set_input_callback(app->view_frame, frame_input_callback);
    view_set_enter_callback(app->view_frame, view_frame_enter_callback);
    view_set_exit_callback(app->view_frame, view_frame_exit_callback);
    view_set_context(app->view_frame, app);
    view_set_custom_callback(app->view_frame, view_custom_event_callback);
    view_allocate_model(app->view_frame, ViewModelTypeLockFree, sizeof(ReqModel));
    ReqModel* frame_model = view_get_model(app->view_frame);
    frame_model->req_sts = false;
    frame_model->last_input = INPUT_RESET;
    frame_model->worker_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    frame_model->url = furi_string_alloc();
    frame_model->curr_cmd = furi_string_alloc();
    furi_string_set_str(frame_model->curr_cmd, "   -> ");
    frame_model->req_path = furi_string_alloc();
    furi_string_set_str(frame_model->req_path, "");
    view_dispatcher_add_view(app->view_dispatcher, ViewFrame, app->view_frame);

    // Home Assistant View
    app->view_ha = view_alloc();
    view_set_draw_callback(app->view_ha, ha_draw_callback);
    view_set_input_callback(app->view_ha, ha_input_callback);
    view_set_enter_callback(app->view_ha, view_ha_enter_callback);
    view_set_exit_callback(app->view_ha, view_ha_exit_callback);
    view_set_context(app->view_ha, app);
    view_set_custom_callback(app->view_ha, view_custom_event_callback);
    view_allocate_model(app->view_ha, ViewModelTypeLockFree, sizeof(ReqModel));
    ReqModel* ha_model = view_get_model(app->view_ha);
    ha_model->req_sts = false;
    ha_model->last_input = INPUT_RESET;
    ha_model->worker_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    ha_model->url = furi_string_alloc();
    ha_model->url_cmd = furi_string_alloc();
    ha_model->token = furi_string_alloc();
    ha_model->curr_cmd = furi_string_alloc();
    ha_model->headers = furi_string_alloc();
    ha_model->payload = furi_string_alloc();
    ha_model->payload_dehum = furi_string_alloc();
    ha_model->print_bedroom_temp = furi_string_alloc();
    ha_model->print_bedroom_hum = furi_string_alloc();
    ha_model->print_kitchen_temp = furi_string_alloc();
    ha_model->print_kitchen_hum = furi_string_alloc();
    ha_model->print_outside_temp = furi_string_alloc();
    ha_model->print_outside_hum = furi_string_alloc();
    ha_model->print_dehum_sts = furi_string_alloc();
    ha_model->print_co2 = furi_string_alloc();
    ha_model->print_pm2_5 = furi_string_alloc();
    ha_model->print_aidx = furi_string_alloc();

    ha_model->curr_page = HaPageFirst;

    view_dispatcher_add_view(app->view_dispatcher, ViewHa, app->view_ha);

    load_settings(app);

    // Variable Items
    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);
    // Frame
    app->frame_path_item = variable_item_list_add(
        app->variable_item_list_config, FRAME_PATH_CONFIG_LABEL, 1, NULL, NULL);
    variable_item_set_current_value_text(
        app->frame_path_item, furi_string_get_cstr(frame_model->url));
    app->frame_ssid_item = variable_item_list_add(
        app->variable_item_list_config, FRAME_SSID_CONFIG_LABEL, 1, NULL, NULL);
    variable_item_set_current_value_text(
        app->frame_ssid_item, furi_string_get_cstr(app->frame_ssid));
    app->pass_frame_item = variable_item_list_add(
        app->variable_item_list_config, FRAME_PASS_CONFIG_LABEL, 1, NULL, NULL);
    variable_item_set_current_value_text(
        app->pass_frame_item, furi_string_get_cstr(app->frame_pass));

    // Home Assistant
    app->ha_path_item = variable_item_list_add(
        app->variable_item_list_config, HA_PATH_CONFIG_LABEL, 1, NULL, NULL);
    variable_item_set_current_value_text(app->ha_path_item, furi_string_get_cstr(ha_model->url));
    app->ha_path_cmd_item = variable_item_list_add(
        app->variable_item_list_config, HA_PATH_CMD_CONFIG_LABEL, 1, NULL, NULL);
    variable_item_set_current_value_text(
        app->ha_path_cmd_item, furi_string_get_cstr(ha_model->url_cmd));
    app->ha_ssid_item = variable_item_list_add(
        app->variable_item_list_config, HA_SSID_CONFIG_LABEL, 1, NULL, NULL);
    variable_item_set_current_value_text(app->ha_ssid_item, furi_string_get_cstr(app->ha_ssid));
    app->pass_ha_item = variable_item_list_add(
        app->variable_item_list_config, HA_PASS_CONFIG_LABEL, 1, NULL, NULL);
    variable_item_set_current_value_text(app->pass_ha_item, furi_string_get_cstr(app->ha_pass));

    // Polling
    app->polling_ha_item = variable_item_list_add(
        app->variable_item_list_config,
        POLLING_CONFIG_LABEL,
        COUNT_OF(polling_values),
        polling_setting_changed,
        app);
    variable_item_set_current_value_index(app->polling_ha_item, ha_model->polling_rate_index);
    variable_item_set_current_value_text(
        app->polling_ha_item, polling_names[ha_model->polling_rate_index]);

    variable_item_list_set_enter_callback(
        app->variable_item_list_config, setting_item_clicked, app);
    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config), navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        ViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    // Resp
    app->text_box_resp = text_box_alloc();
    text_box_set_text(app->text_box_resp, "Response or errors will be shown here after a request");
    view_set_previous_callback(text_box_get_view(app->text_box_resp), navigation_submenu_callback);
    view_set_exit_callback(text_box_get_view(app->text_box_resp), view_resp_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewResp, text_box_get_view(app->text_box_resp));

    // Flipper HTTP
    fhttp = flipper_http_alloc();
    if(fhttp == NULL) {
        FURI_LOG_E(TAG, "Failed to initialize UART");
        return NULL;
    }

    while(fhttp->state == INACTIVE) {
        flipper_http_ping(fhttp);
        furi_delay_ms(furi_ms_to_ticks(100));
    }

    flipper_http_led_off(fhttp);

    app->comm_thread = furi_thread_alloc();
    furi_thread_set_name(app->comm_thread, "Comm_Thread");
    furi_thread_set_stack_size(app->comm_thread, 2048);
    furi_thread_set_context(app->comm_thread, app);

    return app;
}

/**
 * @brief      Free the  application.
 * @details    This function frees the  application resources.
 * @param      app  The  application object.
*/
void app_free(App* app) {
    ReqModel* frame_model = view_get_model(app->view_frame);
    ReqModel* ha_model = view_get_model(app->view_ha);

    if(app->comm_thread) {
        furi_thread_flags_set(app->comm_thread_id, ThreadCommStop);
        furi_thread_join(app->comm_thread);
        furi_thread_free(app->comm_thread);
    }

    flipper_http_free(fhttp);

    furi_mutex_free(app->config_mutex);
    furi_mutex_free(frame_model->worker_mutex);
    furi_mutex_free(ha_model->worker_mutex);

    furi_string_free(ha_model->print_bedroom_temp);
    furi_string_free(ha_model->print_bedroom_hum);
    furi_string_free(ha_model->print_kitchen_temp);
    furi_string_free(ha_model->print_kitchen_hum);
    furi_string_free(ha_model->print_outside_temp);
    furi_string_free(ha_model->print_outside_hum);
    furi_string_free(ha_model->print_co2);
    furi_string_free(ha_model->print_pm2_5);
    furi_string_free(ha_model->print_aidx);

    furi_string_free(app->frame_ssid);
    furi_string_free(app->frame_pass);
    furi_string_free(frame_model->url);
    furi_string_free(frame_model->req_path);
    furi_string_free(frame_model->curr_cmd);

    furi_string_free(app->ha_ssid);
    furi_string_free(app->ha_pass);
    furi_string_free(ha_model->token);
    furi_string_free(ha_model->url);
    furi_string_free(ha_model->url_cmd);
    furi_string_free(ha_model->headers);
    furi_string_free(ha_model->payload);
    furi_string_free(ha_model->payload_dehum);
    furi_string_free(ha_model->curr_cmd);

    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputFramePath);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputFrameSsid);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputFramePass);

    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputHaPath);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputHaPathCmd);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputHaSsid);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputHaPass);

    uart_text_input_free(app->text_input_frame_path);
    uart_text_input_free(app->text_input_frame_ssid);
    uart_text_input_free(app->text_input_frame_pass);

    uart_text_input_free(app->text_input_ha_path);
    uart_text_input_free(app->text_input_ha_path_cmd);
    uart_text_input_free(app->text_input_ha_ssid);
    uart_text_input_free(app->text_input_ha_pass);

    free(app->temp_buffer_frame_path);
    free(app->temp_buffer_frame_ssid);
    free(app->temp_buffer_frame_pass);

    free(app->temp_buffer_ha_path);
    free(app->temp_buffer_ha_path_cmd);
    free(app->temp_buffer_ha_ssid);
    free(app->temp_buffer_ha_pass);

    view_dispatcher_remove_view(app->view_dispatcher, ViewConfigure);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, ViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewFrame);
    view_free(app->view_frame);
    view_dispatcher_remove_view(app->view_dispatcher, ViewHa);
    view_free(app->view_ha);

    text_box_free(app->text_box_resp);
    if(app->formatted_message != NULL) {
        free(app->formatted_message);
    }
    view_dispatcher_remove_view(app->view_dispatcher, ViewResp);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_timer_flush();
    if(app->timer_draw != NULL) {
        furi_timer_stop(app->timer_draw);
        furi_timer_free(app->timer_draw);
    }
    if(app->timer_reset_key != NULL) {
        furi_timer_stop(app->timer_reset_key);
        furi_timer_free(app->timer_reset_key);
    }

    free(app);
}
