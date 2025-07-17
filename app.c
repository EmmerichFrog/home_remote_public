#include "app.h"
#include "alloc_free.h"
#include "frame.h"

//static const char HA_AIDX[] = "aidx";
static const char FRAME_URL_KEY[] = "frame_url";
static const char FRAME_SSID_KEY[] = "frame_ssid";
static const char FRAME_PASS_KEY[] = "frame_pass";

static const char HA_URL_KEY[] = "ha_url";
static const char HA_URL_CMD_KEY[] = "ha_url_cmd";
static const char HA_SSID_KEY[] = "ha_ssid";
static const char HA_PASS_KEY[] = "ha_pass";
static const char HA_POLLING_KEY[] = "ha_polling";
static const char HA_TOKEN_KEY[] = "ha_token";

const uint16_t polling_values[4] = {500U, 1000U, 5000U, 10000U};
const char* polling_names[4] = {"500ms", "1s", "5s", "10s"};

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

        FuriString* json = furi_string_alloc();
        furi_string_set_str(json, "{");

        char json_entry_fmt_s[] = "\n\t\"%s\": \"%s\"";
        char json_entry_fmt_u[] = "\n\t\"%s\": \"%u\"";

        char json_entry[256];

        // Frame Settings
        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_s,
            FRAME_URL_KEY,
            furi_string_get_cstr(frame_model->url));
        furi_string_cat_str(json, json_entry);
        furi_string_cat_str(json, ",");
        json_entry[0] = '\0';

        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_s,
            FRAME_SSID_KEY,
            furi_string_get_cstr(app->frame_ssid));
        furi_string_cat_str(json, json_entry);
        furi_string_cat_str(json, ",");
        json_entry[0] = '\0';

        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_s,
            FRAME_PASS_KEY,
            furi_string_get_cstr(app->frame_pass));
        furi_string_cat_str(json, json_entry);
        furi_string_cat_str(json, ",");
        json_entry[0] = '\0';

        // Home Assistant
        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_s,
            HA_URL_KEY,
            furi_string_get_cstr(ha_model->url));
        furi_string_cat_str(json, json_entry);
        furi_string_cat_str(json, ",");

        json_entry[0] = '\0';

        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_s,
            HA_URL_CMD_KEY,
            furi_string_get_cstr(ha_model->url_cmd));
        furi_string_cat_str(json, json_entry);
        furi_string_cat_str(json, ",");
        json_entry[0] = '\0';

        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_s,
            HA_SSID_KEY,
            furi_string_get_cstr(app->ha_ssid));
        furi_string_cat_str(json, json_entry);
        furi_string_cat_str(json, ",");
        json_entry[0] = '\0';

        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_s,
            HA_PASS_KEY,
            furi_string_get_cstr(app->ha_pass));
        furi_string_cat_str(json, json_entry);
        furi_string_cat_str(json, ",");
        json_entry[0] = '\0';

        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_u,
            HA_POLLING_KEY,
            ha_model->polling_rate_index);
        furi_string_cat_str(json, json_entry);
        furi_string_cat_str(json, ",");
        json_entry[0] = '\0';

        snprintf(
            json_entry,
            sizeof(json_entry),
            json_entry_fmt_s,
            HA_TOKEN_KEY,
            furi_string_get_cstr(ha_model->token));
        furi_string_cat_str(json, json_entry);
        json_entry[0] = '\0';

        furi_string_cat_str(json, "\n}");

        if(storage_file_open(file, RR_CONF_PATH, FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
            storage_file_truncate(file);
            storage_file_write(
                file, furi_string_get_cstr(json), strlen(furi_string_get_cstr(json)));
        } else {
            FURI_LOG_E(TAG, "Error opening %s for writing", RR_CONF_PATH);
        }
        storage_file_close(file);

        storage_file_free(file);
        furi_string_free(json);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_I(TAG, "Saving data completed");
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
    if(!storage_dir_exists(storage, RR_SETTINGS_FOLDER)) {
        FURI_LOG_I(TAG, "Folder missing, creating %s", RR_SETTINGS_FOLDER);
        storage_simply_mkdir(storage, RR_SETTINGS_FOLDER);
    }
    File* file = storage_file_alloc(storage);
    size_t buf_size = 512;
    uint8_t* file_buffer = malloc(buf_size);
    FuriString* json = furi_string_alloc();
    uint16_t max_tokens = 128;

    if(storage_file_open(file, RR_CONF_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, file_buffer, buf_size);
    } else {
        FURI_LOG_E(TAG, "Failed to open config file %s", RR_CONF_PATH);
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
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", FRAME_SSID_KEY);
    }

    value = get_json_value(FRAME_PASS_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(app->frame_pass, value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", FRAME_PASS_KEY);
    }

    // Home Assistant Settings
    value = get_json_value(HA_URL_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(ha_model->url, value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_URL_KEY);
    }

    value = get_json_value(HA_URL_CMD_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(ha_model->url_cmd, value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_URL_CMD_KEY);
    }

    value = get_json_value(HA_SSID_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(app->ha_ssid, value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_SSID_KEY);
    }

    value = get_json_value(HA_PASS_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(app->ha_pass, value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_PASS_KEY);
    }

    value = get_json_value(HA_POLLING_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        ha_model->polling_rate_index = strtoul(value, NULL, 10);
        ha_model->polling_rate = polling_values[ha_model->polling_rate_index];
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", HA_POLLING_KEY);
    }

    value = get_json_value(HA_TOKEN_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        furi_string_set_str(ha_model->token, value);
        ha_model->token_lenght = furi_string_size(ha_model->token);
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
 * @brief      Short buzz the vibration
*/
void buzz_vibration() {
    furi_hal_vibro_on(true);
    furi_delay_ms(50);
    furi_hal_vibro_on(false);
}

/**
 * @brief      Generate a random number bewteen min and max
 * @param      min  minimum value
 * @param      max  maximum value
 * @return     the random number
*/
int32_t random_limit(int32_t min, int32_t max) {
    int32_t rnd = rand() % (max + 1 - min) + min;
    return rnd;
}

/**
 * @brief      Check if sending a request is allowed
 * @param      context  the current context
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
 * @param      context  the current context
 * @param      model  the current model
 * @return     true if it's allowed
*/
static bool allow_back(ReqModel* model) {
    return (fhttp->curr_req_sts == PROCESSING_INACTIVE && !model->req_sts) ||
           fhttp->curr_req_sts == PROCESSING_DONE || fhttp->state == ISSUE;
}

/**
 * @brief      Reverse an uint8_t array
 * @param      arr  pointer to the array to be reversed
 * @param      size  the array size
*/
void reverse_array_uint8(uint8_t* arr, size_t size) {
    uint8_t temp[size];

    for(size_t i = 0; i < size; i++)
        temp[i] = arr[size - i - 1];

    for(size_t i = 0; i < size; i++)
        arr[i] = temp[i];
}

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
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
 *            indicate that we want to navigate to the submenu.
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
 *            indicate that we want to navigate to the configure screen.
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
    default:
        break;
    }
}

/**
 * @brief      Function called when one of the VariableItem is updated.
 * @param      item  The pointer to the VariableItem object.
*/
void polling_setting_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    ReqModel* ha_model = view_get_model(app->view_ha);

    ha_model->polling_rate_index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, polling_names[ha_model->polling_rate_index]);
    ha_model->polling_rate = polling_values[ha_model->polling_rate_index];
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
        FURI_LOG_W(TAG, "Unhandled index [%u] in conf_text_updated.", app->config_index);
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
    const char* src = furi_string_get_cstr(string);
    char* end = memchr(src, '\0', size);
    size_t copy_len = end ? (end - src + 1) : size;
    memcpy(app->temp_buffer, src, copy_len);
    char* p = app->temp_buffer + copy_len;

    if(!p) {
        app->temp_buffer[size] = '\0';
        FURI_LOG_I(
            TAG,
            "[settings_item_clicked]: Manually temrinating string in [temp_buffer], check sizes");
    }
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

/**
 * @brief      Formats the text box string.
 * @details    This function is makes the json in the text box more readable
 * @param      message  The string to format
 * @param      text_box Pointer to the TextBox object
*/
void text_box_format_msg(App* app, const char* message, TextBox* text_box) {
    if(text_box == NULL) {
        FURI_LOG_E(TAG, "Invalid pointer to TextBox");
        return;
    }

    if(message == NULL) {
        FURI_LOG_E(TAG, "Invalid pointer to message");
        return;
    }

    text_box_reset(text_box);

    uint32_t message_length = strlen(message); // Length of the message
    if(message_length > 0) {
        uint32_t i = 0; // Index tracker
        uint32_t formatted_index = 0; // Tracker for where we are in the formatted message
        if(app->formatted_message != NULL) {
            free(app->formatted_message);
            app->formatted_message = NULL;
        }

        if(!easy_flipper_set_buffer(&app->formatted_message, (message_length * 5))) {
            return;
        }

        while(i < message_length) {
            uint32_t max_line_length = 31; // Maximum characters per line
            uint32_t remaining_length = message_length - i; // Remaining characters
            uint32_t line_length = (remaining_length < max_line_length) ? remaining_length :
                                                                          max_line_length;

            // Check for newline character within the current segment
            uint32_t newline_pos = i;
            bool found_newline = false;
            for(; newline_pos < i + line_length && newline_pos < message_length; newline_pos++) {
                if(message[newline_pos] == '\n') {
                    found_newline = true;
                    break;
                }
            }

            if(found_newline) {
                // If newline found, set line_length up to the newline
                line_length = newline_pos - i;
            }

            // Temporary buffer to hold the current line
            char line[32];
            strncpy(line, message + i, line_length);
            line[line_length] = '\0';

            // Move the index forward by the determined line_length
            if(!found_newline) {
                i += line_length;

                // Skip any spaces at the beginning of the next line
                while(i < message_length && message[i] == ' ') {
                    i++;
                }
            }
            uint8_t indent_level = 0;
            // Manually copy the fixed line into the formatted_message buffer, adding newlines where necessary
            for(uint32_t j = 0; j < line_length; j++) {
                switch(line[j]) {
                case '{':
                    app->formatted_message[formatted_index++] = line[j];
                    app->formatted_message[formatted_index++] = '\n';
                    indent_level++;
                    for(size_t k = 0; k < indent_level; k++) {
                        app->formatted_message[formatted_index++] = ' ';
                    }

                    break;
                case ',':
                    app->formatted_message[formatted_index++] = line[j];
                    app->formatted_message[formatted_index++] = '\n';
                    for(size_t k = 0; k < indent_level; k++) {
                        app->formatted_message[formatted_index++] = ' ';
                    }
                    break;
                case '}':
                    app->formatted_message[formatted_index++] = '\n';
                    for(size_t k = 1; k < indent_level; k++) {
                        app->formatted_message[formatted_index++] = ' ';
                    }
                    indent_level--;
                    app->formatted_message[formatted_index++] = line[j];
                    break;
                case '\"':
                    break;
                default:
                    app->formatted_message[formatted_index++] = line[j];
                    break;
                }
            }
        }

        // Null-terminate the formatted_message
        app->formatted_message[formatted_index] = '\0';

        // Add the formatted message to the text box
        text_box_set_text(text_box, app->formatted_message);
        text_box_set_focus(text_box, TextBoxFocusStart);
    } else {
        text_box_set_text(text_box, "No data in payload");
    }
}

/**
 * @brief   Extract a string between the given delimiters
 * @param   string      char* where to search
*  @param   dest        FuriString where to save the found string
 * @param   beginning   first delimiter
 * @param   ending      last delimiter
 * @return  true if the event was handled, false otherwise.
*/
void extract_payload(
    const char* restrict string,
    FuriString* dest,
    const char* restrict beginning,
    const char* restrict ending) {
    //
    char *start, *end;
    char* temp;
    temp = NULL;
    bool failed = false;
    start = strstr(string, beginning);
    end = strstr(start, ending);
    if(start && end) {
        temp = (char*)malloc(end - start + 1);

        if(start) {
            start += strlen(beginning);
            if(end) {
                size_t len = end - start;
		char* endptr = memchr(start, '\0', len);
		size_t copy_len;
		if(endptr) {
    		    copy_len = (size_t)(endptr - start + 1);
		} else {
    		    copy_len = len;
		}
		memcpy(temp, start, copy_len);
		char* p = temp + copy_len;
                if(!p) {
                    temp[end - start] = '\0';
                    FURI_LOG_I(
                        TAG,
                        "[extract_payload]: Manually temrinating string in [temp], check sizes");
                }
            } else {
                failed = true;
            }
        } else {
            failed = true;
        }
    } else {
        failed = true;
    }

    if(!failed) {
        furi_string_set_str(dest, temp);

    } else {
        furi_string_set_str(dest, "cannot extract payload");
    }

    free(temp);
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
    //BtTest* bt_model = view_get_model(app->view_bt);

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
    case EventIdBtCheckBack:
        if(true) {
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
int32_t rest_remote_app(void* _p) {
    UNUSED(_p);
    furi_hal_gpio_init_simple(pin_wake, GpioModeOutputPushPull);
    furi_hal_gpio_write(pin_wake, true);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    furi_hal_gpio_write(pin_wake, false);
    furi_delay_ms(100);
    furi_hal_gpio_init_simple(pin_wake, GpioModeAnalog);
    return 0;
}
