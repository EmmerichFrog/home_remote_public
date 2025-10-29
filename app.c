#include "app.h"
#include "src/alloc_free.h"
#include "libs/jsmn.h"
#include <storage/storage.h>

const uint16_t polling_values[4] = {500U, 1000U, 5000U, 10000U};
const char* polling_names[4] = {"500ms", "1s", "5s", "10s"};
const char* ctrl_mode_names[3] = {"Wifi", "Sghz+BT Home", "Bt Serial"};
const char* randomize_mac_names[2] = {"Off", "On"};

static const char FRAME_URL_KEY[] = "frame_url";
static const char FRAME_SSID_KEY[] = "frame_ssid";
static const char FRAME_PASS_KEY[] = "frame_pass";

static const char HA_URL_KEY[] = "ha_url";
static const char HA_URL_CMD_KEY[] = "ha_url_cmd";
static const char HA_SSID_KEY[] = "ha_ssid";
static const char HA_PASS_KEY[] = "ha_pass";
static const char HA_POLLING_KEY[] = "ha_polling";
static const char HA_CTRL_MODE_KEY[] = "ha_ctrl";
static const char RANDOMIZE_MAC_KEY[] = "bt_randomize_mac";
static const char HA_TOKEN_KEY[] = "ha_token";

//This pin will be set to 1 to wake the board when the app is in use
const GpioPin* const pin_wake = &gpio_ext_pa4;

FlipperHTTP* fhttp;
/**
 * @brief      Save path, ssid and password to file on change.
 * @param      app  The context
*/
void save_settings(App* app) {
    if(furi_mutex_acquire(app->config_mutex, FuriWaitForever) == FuriStatusOk) {
        FURI_LOG_I(TAG, "Saving json config...");
        ReqModel* frame_model = view_get_model(app->view_frame);
        ReqModel* ha_model = view_get_model(app->view_ha);

        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* file = storage_file_alloc(storage);

        FuriJson* json = furi_json_alloc();

        // Frame Settings
        furi_json_add_entry(json, FRAME_URL_KEY, furi_string_get_cstr(frame_model->url));
        furi_json_add_entry(json, FRAME_SSID_KEY, furi_string_get_cstr(app->frame_ssid));
        furi_json_add_entry(json, FRAME_PASS_KEY, furi_string_get_cstr(app->frame_pass));

        // Home Assistant
        furi_json_add_entry(json, HA_URL_KEY, furi_string_get_cstr(ha_model->url));
        furi_json_add_entry(json, HA_URL_CMD_KEY, furi_string_get_cstr(ha_model->url_cmd));
        furi_json_add_entry(json, HA_SSID_KEY, furi_string_get_cstr(app->ha_ssid));
        furi_json_add_entry(json, HA_PASS_KEY, furi_string_get_cstr(app->ha_pass));
        furi_json_add_entry(json, HA_POLLING_KEY, (uint32_t)ha_model->polling_rate_index);
        furi_json_add_entry(json, HA_CTRL_MODE_KEY, (uint32_t)ha_model->control_mode);
        furi_json_add_entry(json, RANDOMIZE_MAC_KEY, (uint32_t)ha_model->ble->randomize_mac_enb);

        furi_json_add_entry(json, HA_TOKEN_KEY, furi_string_get_cstr(ha_model->token));

        size_t len_w = 0;
        size_t len_req = strlen(json->to_text);
        if(storage_file_open(file, HR_CONF_PATH, FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
            storage_file_truncate(file);
            len_w = storage_file_write(file, json->to_text, len_req);
        } else {
            FURI_LOG_E(TAG, "Error opening %s for writing", HR_CONF_PATH);
        }
        storage_file_close(file);

        storage_file_free(file);
        furi_json_free(json);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_I(TAG, "Saving data completed, written %u bytes, buffer was %u", len_w, len_req);
        furi_check(furi_mutex_release(app->config_mutex) == FuriStatusOk);
    }
}

/**
 * @brief      Load path, ssid and password from file on start if available, otherwise set some default values.
 * @param      app  The context
*/
void load_settings(App* app) {
    FURI_LOG_I(TAG, "Loading json config...");

    ReqModel* frame_model = view_get_model(app->view_frame);
    ReqModel* ha_model = view_get_model(app->view_ha);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_dir_exists(storage, HR_SETTINGS_FOLDER)) {
        FURI_LOG_I(TAG, "Folder missing, creating %s", HR_SETTINGS_FOLDER);
        storage_simply_mkdir(storage, HR_SETTINGS_FOLDER);
    }
    File* file = storage_file_alloc(storage);
    size_t buf_size = 768;
    uint8_t* file_buffer = malloc(buf_size);
    FuriString* json = furi_string_alloc();
    uint16_t max_tokens = 128;

    if(storage_file_open(file, HR_CONF_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, file_buffer, buf_size);
    } else {
        FURI_LOG_E(TAG, "Failed to open config file %s", HR_CONF_PATH);
    }
    storage_file_close(file);

    for(size_t i = 0; i < buf_size; i++) {
        furi_string_push_back(json, file_buffer[i]);
    }

    // Frame Settings
    char* value;
    value = get_json_value(FRAME_URL_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(frame_model->url, value);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", FRAME_URL_KEY);
    }

    value = get_json_value(FRAME_SSID_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(app->frame_ssid, value);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", FRAME_SSID_KEY);
    }

    value = get_json_value(FRAME_PASS_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(app->frame_pass, value);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", FRAME_PASS_KEY);
    }

    // Home Assistant Settings
    value = get_json_value(HA_URL_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(ha_model->url, value);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_URL_KEY);
    }

    value = get_json_value(HA_URL_CMD_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(ha_model->url_cmd, value);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_URL_CMD_KEY);
    }

    value = get_json_value(HA_SSID_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(app->ha_ssid, value);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_SSID_KEY);
    }

    value = get_json_value(HA_PASS_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(app->ha_pass, value);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_PASS_KEY);
    }

    value = get_json_value(HA_POLLING_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        ha_model->polling_rate_index = strtoul(value, NULL, 10);
        ha_model->polling_rate = polling_values[ha_model->polling_rate_index];
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_POLLING_KEY);
    }

    value = get_json_value(HA_CTRL_MODE_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        ha_model->control_mode = strtoul(value, NULL, 10);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_POLLING_KEY);
    }
    value = get_json_value(RANDOMIZE_MAC_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        uint32_t index = strtoul(value, NULL, 10);
        switch(index) {
        case 0:
            ha_model->ble->randomize_mac_enb = false;
            break;
        case 1:
            ha_model->ble->randomize_mac_enb = true;
            break;
        default:
            break;
        }
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", RANDOMIZE_MAC_KEY);
    }

    value = get_json_value(HA_TOKEN_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(ha_model->token, value);
        ha_model->token_lenght = furi_string_size(ha_model->token);
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", FRAME_URL_KEY);
    }

    free(file_buffer);
    furi_string_free(json);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "Loading data completed");
}

/**
 * @brief      Check if sending a request is allowed
 * @param      model  the current model
 * @return     true if it's allowed
*/
bool allow_cmd(ReqModel* model) {
    return ((fhttp->curr_req_sts == PROCESSING_INACTIVE || fhttp->state == ISSUE) &&
            !model->req_sts) ||
           fhttp->curr_req_sts == PROCESSING_DONE;
}

/**
 * @brief      Check if exiting the view is allowed
 * @param      model  the current model
 * @return     true if it's allowed
*/
static bool allow_back(ReqModel* model) {
    return (fhttp->curr_req_sts == PROCESSING_INACTIVE && !model->req_sts) ||
           fhttp->curr_req_sts == PROCESSING_DONE || fhttp->state == ISSUE;
}

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *             indicate that we want to exit the application.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return ViewSubmenu to
 *             indicate that we want to navigate to the submenu.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_submenu_callback(void* context) {
    UNUSED(context);
    return ViewSubmenu;
}

/**
 * @brief      Callback for returning to configure screen.
 * @details    This function is called when user press back button.  We return ViewConfigure to
 *             indicate that we want to navigate to the configure screen.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_configure_callback(void* context) {
    UNUSED(context);
    return ViewConfigure;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - App object.
 * @param      index     The SubmenuIndex item that was clicked.
*/
void submenu_callback(void* context, uint32_t index) {
    App* app = (App*)context;
    switch(index) {
    case SubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewConfigure);
        break;
    case SubmenuIndexFrame:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewFrame);
        break;
    case SubmenuIndexHa:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewHa);
        break;
    case SubmenuIndexResp:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewResp);
        break;
    case SubmenuIndexAbout:
        uint32_t rnd = futils_random_limit(DUDUBUBU_FIRST, DUDUBUBU_LAST);
        widget_reset(app->widget_about);

        switch(rnd) {
        case DUDUBUBU_BUTT:
            widget_add_icon_element(app->widget_about, 19, 0, &I_dudububu_butt);
            break;
        case DUDUBUBU_HUG:
            widget_add_icon_element(app->widget_about, 32, 0, &I_dudububu_hug);
            break;
        default:
            break;
        }

        view_dispatcher_switch_to_view(app->view_dispatcher, ViewAbout);
        break;
    default:
        break;
    }
}

/**
 * @brief      Function called when one of the VariableItem is updated.
 * @param      item  The pointer to the VariableItem object.
*/
void variable_item_setting_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    ReqModel* ha_model = view_get_model(app->view_ha);

    uint8_t index = variable_item_list_get_selected_item_index(app->variable_item_list_config);
    FURI_LOG_I(TAG, "Index %u", index);
    switch(index) {
    case ConfigVariableItemPolling:
        ha_model->polling_rate_index = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(item, polling_names[ha_model->polling_rate_index]);
        ha_model->polling_rate = polling_values[ha_model->polling_rate_index];
        break;
    case ConfigVariableItemCtrlMode:
        ha_model->control_mode = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(item, ctrl_mode_names[ha_model->control_mode]);
        break;

    case ConfigVariableItemRandomizeMac:
        ha_model->ble->randomize_mac_enb = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(
            item, randomize_mac_names[variable_item_get_current_value_index(item)]);
        break;

    default:
        FURI_LOG_E(TAG, "Unhandled index [%u] in variable_item_setting_changed.", index);
        return;
    }
    save_settings(app);
}

/**
 * @brief      Function called when one of the texts is updated.
 * @param      context  The context - App object.
*/
void conf_text_updated(void* context) {
    App* app = (App*)context;
    ReqModel* frame_model = view_get_model(app->view_frame);
    ReqModel* ha_model = view_get_model(app->view_ha);
    uint8_t upd_flag = 0;

    switch(app->config_index) {
    // Frame
    case ConfigTextInputFramePath:
        furi_string_set_str(frame_model->url, app->temp_buffer_frame_path);
        variable_item_set_current_value_text(
            app->frame_path_item, furi_string_get_cstr(frame_model->url));
        break;
    case ConfigTextInputFrameSsid:
        furi_string_set_str(app->frame_ssid, app->temp_buffer_frame_ssid);
        variable_item_set_current_value_text(
            app->frame_ssid_item, furi_string_get_cstr(app->frame_ssid));
        upd_flag = 1;
        break;
    case ConfigTextInputFramePass:
        furi_string_set_str(app->frame_pass, app->temp_buffer_frame_pass);
        variable_item_set_current_value_text(
            app->pass_frame_item, furi_string_get_cstr(app->frame_pass));
        upd_flag = 1;
        break;
    // Home Assistant
    case ConfigTextInputHaPath:
        furi_string_set_str(ha_model->url, app->temp_buffer_ha_path);
        variable_item_set_current_value_text(
            app->ha_path_item, furi_string_get_cstr(ha_model->url));
        break;
    case ConfigTextInputHaPathCmd:
        furi_string_set_str(ha_model->url_cmd, app->temp_buffer_ha_path_cmd);
        variable_item_set_current_value_text(
            app->ha_path_cmd_item, furi_string_get_cstr(ha_model->url_cmd));
        break;
    case ConfigTextInputHaSsid:
        furi_string_set_str(app->ha_ssid, app->temp_buffer_ha_ssid);
        variable_item_set_current_value_text(
            app->ha_ssid_item, furi_string_get_cstr(app->ha_ssid));
        upd_flag = 2;
        break;
    case ConfigTextInputHaPass:
        furi_string_set_str(app->ha_pass, app->temp_buffer_ha_pass);
        variable_item_set_current_value_text(
            app->pass_ha_item, furi_string_get_cstr(app->ha_pass));
        upd_flag = 2;
        break;
    default:
        FURI_LOG_E(TAG, "Unhandled index [%u] in conf_text_updated.", app->config_index);
        return;
    }

    switch(upd_flag) {
    case 1U:
        flipper_http_save_wifi(
            fhttp, furi_string_get_cstr(app->frame_ssid), furi_string_get_cstr(app->frame_pass));
        break;
    case 2U:
        flipper_http_save_wifi(
            fhttp, furi_string_get_cstr(app->ha_ssid), furi_string_get_cstr(app->ha_pass));
        break;

    default:
        break;
    }

    save_settings(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewConfigure);
}

/**
 * @brief      Callback when item in configuration screen is clicked.
 * @details    This function is called when user clicks OK on an item in the configuration screen.
 *             If the item clicked is a text field then we switch to the text input screen.
 * @param      context  The context - App object.
 * @param      index - The index of the item that was clicked.
*/
void setting_item_clicked(void* context, uint32_t index) {
    App* app = (App*)context;
    ReqModel* frame_model = view_get_model(app->view_frame);
    ReqModel* ha_model = view_get_model(app->view_ha);
    uint16_t size;
    FuriString* string;
    UART_TextInput* text_input;
    uint8_t view_index;

    switch(index) {
    // Frame
    case ConfigTextInputFramePath:
        app->temp_buffer = app->temp_buffer_frame_path;
        size = app->temp_buffer_frame_path_size;
        string = frame_model->url;
        text_input = app->text_input_frame_path;
        view_index = ViewTextInputFramePath;
        break;
    case ConfigTextInputFrameSsid:
        app->temp_buffer = app->temp_buffer_frame_ssid;
        size = app->temp_buffer_frame_ssid_size;
        string = app->frame_ssid;
        text_input = app->text_input_frame_ssid;
        view_index = ViewTextInputFrameSsid;
        break;
    case ConfigTextInputFramePass:
        app->temp_buffer = app->temp_buffer_frame_pass;
        size = app->temp_buffer_frame_pass_size;
        string = app->frame_pass;
        text_input = app->text_input_frame_pass;
        view_index = ViewTextInputFramePass;
        break;
    // Home Assistant
    case ConfigTextInputHaPath:
        app->temp_buffer = app->temp_buffer_ha_path;
        size = app->temp_buffer_ha_path_size;
        string = ha_model->url;
        text_input = app->text_input_ha_path;
        view_index = ViewTextInputHaPath;
        break;
    case ConfigTextInputHaPathCmd:
        app->temp_buffer = app->temp_buffer_ha_path_cmd;
        size = app->temp_buffer_ha_path_cmd_size;
        string = ha_model->url_cmd;
        text_input = app->text_input_ha_path_cmd;
        view_index = ViewTextInputHaPathCmd;
        break;
    case ConfigTextInputHaSsid:
        app->temp_buffer = app->temp_buffer_ha_ssid;
        size = app->temp_buffer_ha_ssid_size;
        string = app->ha_ssid;
        text_input = app->text_input_ha_ssid;
        view_index = ViewTextInputHaSsid;
        break;
    case ConfigTextInputHaPass:
        app->temp_buffer = app->temp_buffer_ha_pass;
        size = app->temp_buffer_ha_pass_size;
        string = app->ha_pass;
        text_input = app->text_input_ha_pass;
        view_index = ViewTextInputHaPass;
        break;
    default:
        // don't handle presses that are not explicitly defined in the enum
        return;
    }
    // Initialize temp_buffer with existing string
    futils_copy_str(
        app->temp_buffer,
        furi_string_get_cstr(string),
        size,
        "setting_items_clicked",
        "app->temp_buffer");

    // Configure the text input
    uart_text_input_set_result_callback(
        text_input, conf_text_updated, app, app->temp_buffer, size, false);

    // Set the previous callback to return to Configure screen
    view_set_previous_callback(
        uart_text_input_get_view(text_input), navigation_configure_callback);
    app->config_index = index;
    // Show text input dialog.
    view_dispatcher_switch_to_view(app->view_dispatcher, view_index);
}

/**
 * @brief      Callback to fire data update.
 * @details    This function is called when the timer ticks.
 * @param      context  The context - App object.
*/
void comm_thread_timer_callback(void* context) {
    App* app = (App*)context;
    furi_thread_flags_set(app->comm_thread_id, ThreadCommUpdData);
}

/*
 * @brief      Callback of the response screen on exit.
 * @details    Free the formatted message on exit.
 * @param      context  The context - App object.
*/
void view_resp_exit_callback(void* context) {
    App* app = (App*)context;
    if(app->formatted_message) {
        free(app->formatted_message);
        app->formatted_message = NULL;
    }
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - EventId value.
 * @param      context  The context - App object.
*/
bool view_custom_event_callback(uint32_t event, void* context) {
    App* app = (App*)context;
    ReqModel* frame_model = view_get_model(app->view_frame);
    ReqModel* ha_model = view_get_model(app->view_ha);
    //    SghzTest* sghz_model = view_get_model(app->view_sghz);

    switch(event) {
    // Redraw the screen, called by the timer callback every timer tick
    case EventIdFrameRedrawScreen: {
        with_view_model(app->view_frame, ReqModel * model, { UNUSED(model); }, true);
        return true;
    }
    case EventIdHaRedrawScreen: {
        with_view_model(app->view_ha, ReqModel * model, { UNUSED(model); }, true);
        return true;
    }
    // Called to check if it's allowed to leave the get screen (ie the get request is done and payload is formatted)
    case EventIdFrameCheckBack:
        if(allow_back(frame_model)) {
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        }
        return true;
    case EventIdHaCheckBack:
        if(allow_back(ha_model) || true) {
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        }
        return true;
    case EventIdForceBack:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Callback of the timer_reset to update the btton pressed graphics.
 * @details    This function is called when the timer_reset ticks.
 * @param      context  The context - App object.
*/
void view_timer_key_reset_callback(void* context) {
    App* app = (App*)context;
    switch(app->current_view) {
    case ViewFrame:
        ReqModel* frame_model = view_get_model(app->view_frame);
        frame_model->last_input = INPUT_RESET;
        break;
    case ViewHa:
        ReqModel* ha_model = view_get_model(app->view_ha);
        ha_model->last_input = INPUT_RESET;
        break;
    default:
        break;
    }
}

/**
 * @brief      Main function for  application.
 * @details    This function is the entry point for the  application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t home_remote_app(void* _p) {
    UNUSED(_p);
    furi_hal_random_init();
    furi_hal_gpio_init_simple(pin_wake, GpioModeOutputPushPull);
    furi_hal_gpio_write(pin_wake, true);
    furi_delay_ms(100);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    furi_hal_gpio_write(pin_wake, false);
    furi_delay_ms(100);
    furi_hal_gpio_init_simple(pin_wake, GpioModeAnalog);
    return 0;
}
