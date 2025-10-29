#include "alloc_free.h"
#include "ble_beacon.h"
#include "frame.h"
#include "ha.h"
#include "libs/furi_utils.h"

static const char FRAME_PATH_CONFIG_LABEL[] = "Frame URL";
static const char FRAME_SSID_CONFIG_LABEL[] = "Frame SSID";
static const char FRAME_PASS_CONFIG_LABEL[] = "Frame Pass.";

static const char HA_PATH_CONFIG_LABEL[] = "HA Sens. URL";
static const char HA_PATH_CMD_CONFIG_LABEL[] = "HA Cmd URL";
static const char HA_SSID_CONFIG_LABEL[] = "HA SSID";
static const char HA_PASS_CONFIG_LABEL[] = "HA Pass.";
static const char* POLLING_CONFIG_LABEL = "Polling";
static const char* CTRL_MODE_CONFIG_LABEL = "Ctrl. Mode";
static const char* RANDOMIZE_MAC_LABEL = "Randomize MAC";

extern FlipperHTTP* fhttp;

extern const uint16_t polling_values[4];
extern const char* polling_names[4];
extern const char* ctrl_mode_names[3];
extern const char* randomize_mac_names[2];

/**
 * @brief      Allocate the application.
 * @details    This function allocates the application resources.
 * @return     App object.
*/
App* app_alloc() {
    App* app = malloc(sizeof(App));

    Gui* gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    app->config_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Home Remote");
    submenu_add_item(app->submenu, "Config", SubmenuIndexConfigure, submenu_callback, app);
    submenu_add_item(app->submenu, "Frame Remote", SubmenuIndexFrame, submenu_callback, app);
    submenu_add_item(app->submenu, "Home Assistant", SubmenuIndexHa, submenu_callback, app);
    submenu_add_item(app->submenu, "Response", SubmenuIndexResp, submenu_callback, app);
    submenu_add_item(app->submenu, "About", SubmenuIndexAbout, submenu_callback, app);
    submenu_set_selected_item(app->submenu, SubmenuIndexHa);
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
    app->temp_buffer_frame_path = malloc(app->temp_buffer_frame_path_size);
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
    app->temp_buffer_frame_ssid = malloc(app->temp_buffer_frame_ssid_size);
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
    app->temp_buffer_frame_pass = malloc(app->temp_buffer_frame_pass_size);
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
    app->temp_buffer_ha_path = malloc(app->temp_buffer_ha_path_size);

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
    app->temp_buffer_ha_path_cmd = malloc(app->temp_buffer_ha_path_cmd_size);

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
    app->temp_buffer_ha_ssid = malloc(app->temp_buffer_ha_ssid_size);
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
    app->temp_buffer_ha_pass = malloc(app->temp_buffer_ha_pass_size);
    app->ha_pass = furi_string_alloc();

    // Frame View
    app->view_frame = view_alloc();
    view_set_draw_callback(app->view_frame, frame_draw_callback);
    view_set_input_callback(app->view_frame, frame_input_callback);
    view_set_enter_callback(app->view_frame, frame_enter_callback);
    view_set_exit_callback(app->view_frame, frame_exit_callback);
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
    view_set_enter_callback(app->view_ha, ha_enter_callback);
    view_set_exit_callback(app->view_ha, ha_exit_callback);
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
    ha_model->curr_page = PageFirst;
    ha_model->sghz = malloc(sizeof(SghzComm));
    ha_model->sghz->worker_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    ha_model->sghz->last_counter = 0;

    view_dispatcher_add_view(app->view_dispatcher, ViewHa, app->view_ha);

    // Resp
    app->text_box_resp = text_box_alloc();
    text_box_set_text(app->text_box_resp, "Response or errors will be shown here after a request");
    view_set_previous_callback(text_box_get_view(app->text_box_resp), navigation_submenu_callback);
    view_set_exit_callback(text_box_get_view(app->text_box_resp), view_resp_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewResp, text_box_get_view(app->text_box_resp));

    // BT Beacon
    ha_model->ble = malloc(sizeof(BtBeacon));
    ha_model->ble->mac_address_str = furi_string_alloc();
    ha_model->ble->cnt = 0;
    ha_model->ble->config.adv_channel_map = GapAdvChannelMapAll;
    ha_model->ble->config.adv_power_level = GapAdvPowerLevel_6dBm;
    ha_model->ble->config.address_type = GapAddressTypePublic;
    ha_model->ble->beacon_period = BEACON_PERIOD;
    ha_model->ble->beacon_duration = BEACON_DURATION;
    ha_model->ble->default_name_len = strlen(furi_hal_version_get_device_name_ptr());
    ha_model->ble->default_device_name = furi_hal_version_get_device_name_ptr();
    ha_model->ble->device_name = malloc(MAX_NAME_LENGHT + 1);
    futils_copy_str(
        ha_model->ble->device_name,
        ha_model->ble->default_device_name,
        ha_model->ble->default_name_len + 1,
        "app_alloc",
        "ha_model->ble->device_name");

    ha_model->ble->device_name[ha_model->ble->default_name_len] = '\0';
    ha_model->ble->device_name_len = strlen(ha_model->ble->default_device_name) + 1;
    ha_model->ble->curr_page = PageFirst;
    FURI_LOG_I(
        BT_TAG,
        "Device Name: %s, Size: %u",
        ha_model->ble->device_name,
        ha_model->ble->device_name_len);
    const GapExtraBeaconConfig* prev_cfg_ptr = furi_hal_bt_extra_beacon_get_config();
    if(prev_cfg_ptr) {
        ha_model->ble->prev_exists = true;
        memcpy(&ha_model->ble->prev_config, prev_cfg_ptr, sizeof(ha_model->ble->prev_config));
    } else {
        ha_model->ble->prev_exists = false;
    }

    ha_model->ble->prev_data_len = furi_hal_bt_extra_beacon_get_data(ha_model->ble->prev_data);
    ha_model->ble->prev_active = furi_hal_bt_extra_beacon_is_active();

    // BT Serial
    ha_model->bt_serial = malloc(sizeof(BtSerial));
    ha_model->bt_serial->bt = furi_record_open(RECORD_BT);

    load_settings(app);

    // Variable Items
    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);
    variable_item_list_set_header(app->variable_item_list_config, "Configuration");

    // Frame
    app->frame_path_item = futils_variable_item_init(
        app->variable_item_list_config,
        FRAME_PATH_CONFIG_LABEL,
        furi_string_get_cstr(frame_model->url),
        1,
        0,
        NULL,
        NULL);

    app->frame_ssid_item = futils_variable_item_init(
        app->variable_item_list_config,
        FRAME_SSID_CONFIG_LABEL,
        furi_string_get_cstr(app->frame_ssid),
        1,
        0,
        NULL,
        NULL);

    app->pass_frame_item = futils_variable_item_init(
        app->variable_item_list_config,
        FRAME_PASS_CONFIG_LABEL,
        furi_string_get_cstr(app->frame_pass),
        1,
        0,
        NULL,
        NULL);

    // Home Assistant
    app->ha_path_item = futils_variable_item_init(
        app->variable_item_list_config,
        HA_PATH_CONFIG_LABEL,
        furi_string_get_cstr(ha_model->url),
        1,
        0,
        NULL,
        NULL);

    app->ha_path_cmd_item = futils_variable_item_init(
        app->variable_item_list_config,
        HA_PATH_CMD_CONFIG_LABEL,
        furi_string_get_cstr(ha_model->url_cmd),
        1,
        0,
        NULL,
        NULL);

    app->ha_ssid_item = futils_variable_item_init(
        app->variable_item_list_config,
        HA_SSID_CONFIG_LABEL,
        furi_string_get_cstr(app->ha_ssid),
        1,
        0,
        NULL,
        NULL);

    app->pass_ha_item = futils_variable_item_init(
        app->variable_item_list_config,
        HA_PASS_CONFIG_LABEL,
        furi_string_get_cstr(app->ha_pass),
        1,
        0,
        NULL,
        NULL);

    // Polling
    app->polling_ha_item = futils_variable_item_init(
        app->variable_item_list_config,
        POLLING_CONFIG_LABEL,
        polling_names[ha_model->polling_rate_index],
        COUNT_OF(polling_values),
        ha_model->polling_rate_index,
        variable_item_setting_changed,
        app);

    // Control Mode
    app->ctrl_mode_ha_item = futils_variable_item_init(
        app->variable_item_list_config,
        CTRL_MODE_CONFIG_LABEL,
        ctrl_mode_names[ha_model->control_mode],
        COUNT_OF(ctrl_mode_names),
        ha_model->control_mode,
        variable_item_setting_changed,
        app);

    // Randomize MAC
    app->randomize_mac_enb_item = futils_variable_item_init(
        app->variable_item_list_config,
        RANDOMIZE_MAC_LABEL,
        randomize_mac_names[ha_model->ble->randomize_mac_enb],
        COUNT_OF(randomize_mac_names),
        ha_model->ble->randomize_mac_enb,
        variable_item_setting_changed,
        app);

    variable_item_list_set_enter_callback(
        app->variable_item_list_config, setting_item_clicked, app);
    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config), navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        ViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    // About
    easy_flipper_set_widget(
        &app->widget_about, ViewAbout, NULL, navigation_submenu_callback, &app->view_dispatcher);

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

    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_check(furi_hal_bt_extra_beacon_stop());
    }

    if(ha_model->ble->prev_exists) {
        furi_check(furi_hal_bt_extra_beacon_set_config(&ha_model->ble->prev_config));
        furi_check(furi_hal_bt_extra_beacon_set_data(
            ha_model->ble->prev_data, ha_model->ble->prev_data_len));
    }

    if(ha_model->ble->prev_active) {
        furi_check(furi_hal_bt_extra_beacon_start());
    }

    flipper_http_free(fhttp);

    furi_mutex_free(app->config_mutex);
    furi_mutex_free(frame_model->worker_mutex);
    furi_mutex_free(ha_model->worker_mutex);
    furi_mutex_free(ha_model->sghz->worker_mutex);

    furi_string_free(ha_model->print_bedroom_temp);
    furi_string_free(ha_model->print_bedroom_hum);
    furi_string_free(ha_model->print_kitchen_temp);
    furi_string_free(ha_model->print_kitchen_hum);
    furi_string_free(ha_model->print_outside_temp);
    furi_string_free(ha_model->print_outside_hum);
    furi_string_free(ha_model->print_dehum_sts);
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

    furi_string_free(ha_model->ble->mac_address_str);

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

    view_dispatcher_remove_view(app->view_dispatcher, ViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, ViewConfigure);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, ViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewFrame);
    view_free(app->view_frame);
    view_dispatcher_remove_view(app->view_dispatcher, ViewHa);
    free(ha_model->sghz);
    view_free(app->view_ha);

    text_box_free(app->text_box_resp);
    if(app->formatted_message != NULL) {
        free(app->formatted_message);
    }
    view_dispatcher_remove_view(app->view_dispatcher, ViewResp);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);

    free(ha_model->ble);
    free(ha_model->bt_serial);
    free(app);
}
