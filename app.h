#pragma once
#include "rest_remote_icons.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <libs/easy_flipper.h>
#include <libs/flipper_http.h>

#define TAG                 "REST_REMOTE"
#define RR_APPS_DATA_FOLDER EXT_PATH("apps_data")
#define RR_SETTINGS_FOLDER  \
    RR_APPS_DATA_FOLDER "/" \
                        "rest_remote"
#define RR_CONF_FILE_NAME "conf.json"
#define RR_CONF_PATH      RR_SETTINGS_FOLDER "/" RR_CONF_FILE_NAME

#define INPUT_RESET 99

//This pin will be set to 1 to wake the board when the app is in use
extern const GpioPin* const pin_wake;

typedef enum {
    SubmenuIndexConfigure,
    SubmenuIndexFrame,
    SubmenuIndexHa,
    SubmenuIndexResp,
    SubmenuIndexAbout,
} SubmenuIndex;

typedef enum {
    ViewSubmenu, // The menu when the app starts
    ViewTextInputFramePath, // Input for configuring path
    ViewTextInputFrameSsid, // Input for configuring text settings
    ViewTextInputFramePass, // Input for configuring text settings
    ViewTextInputHaPath, // Input for configuring path
    ViewTextInputHaPathCmd, // Input for configuring path
    ViewTextInputHaSsid, // Input for configuring text settings
    ViewTextInputHaPass, // Input for configuring text settings
    ViewConfigure, // The configuration screen
    ViewFrame,
    ViewHa,
    ViewResp,
    ViewAbout,
} ViewEnum;

typedef enum {
    ConfigTextInputFramePath, // Input for configuring path
    ConfigTextInputFrameSsid, // Input for configuring text settings
    ConfigTextInputFramePass, // Input for configuring text settings
    ConfigTextInputHaPath, // Input for configuring path
    ConfigTextInputHaPathCmd, // Input for configuring path
    ConfigTextInputHaSsid, // Input for configuring text settings
    ConfigTextInputHaPass, // Input for configuring text settings
} ConfigIndex;

typedef enum {
    EventIdFrameRedrawScreen = 1, // Custom event to redraw the screen
    EventIdHaRedrawScreen = 2, // Custom event to redraw the screen
    EventIdBtRedrawScreen = 3, // Custom event to redraw the screen
    EventIdFrameCheckBack = 21,
    EventIdHaCheckBack = 22,
    EventIdBtCheckBack = 23,
    EventIdForceBack = 29,
    EventIdPrevAbout = 51,
    EventIdNextAbout = 52,
} EventId;

typedef enum {
    PROCESSING_INACTIVE, // Inactive state
    PROCESSING_BUSY, // Receiving data
    PROCESSING_DONE, // Receiving data done
} ProccessingState;

typedef enum {
    ThreadCommStop = 0b00000001,
    ThreadCommUpdData = 0b00000010,
    ThreadCommSendCmd = 0b00000100,
    ThreadCommStopCmd = 0b00001000,
} EventCommReq;

typedef enum {
    HaPageFirst,
    HaPageSecond,
    HaPageLast,
} HaPageIndex;

typedef struct App {
    ViewDispatcher* view_dispatcher; // Switches between our views
    Submenu* submenu; // The application menu
    VariableItemList* variable_item_list_config; // The configuration screen
    View* view_frame;
    View* view_ha;
    uint8_t current_view;
    TextBox* text_box_resp;

    char* formatted_message;

    uint8_t config_index; //Index for what text input changed
    char* temp_buffer; // Temporary buffer for text input - common

    UART_TextInput* text_input_frame_path; // The text input screen
    VariableItem* frame_path_item; // The name setting item (so we can update the text)
    char* temp_buffer_frame_path; // Temporary buffer for text input
    uint16_t temp_buffer_frame_path_size; // Size of temporary buffer

    UART_TextInput* text_input_frame_ssid; // The text input screen
    VariableItem* frame_ssid_item; // The name setting item (so we can update the text)
    char* temp_buffer_frame_ssid; // Temporary buffer for text input
    uint16_t temp_buffer_frame_ssid_size; // Size of temporary buffer
    FuriString* frame_ssid;

    UART_TextInput* text_input_frame_pass; // The text input screen
    VariableItem* pass_frame_item; // The name setting item (so we can update the text)
    char* temp_buffer_frame_pass; // Temporary buffer for text input
    uint16_t temp_buffer_frame_pass_size; // Size of temporary buffer
    FuriString* frame_pass;

    UART_TextInput* text_input_ha_path; // The text input screen
    VariableItem* ha_path_item; // The name setting item (so we can update the text)
    char* temp_buffer_ha_path; // Temporary buffer for text input
    uint16_t temp_buffer_ha_path_size; // Size of temporary buffer

    UART_TextInput* text_input_ha_path_cmd; // The text input screen
    VariableItem* ha_path_cmd_item; // The name setting item (so we can update the text)
    char* temp_buffer_ha_path_cmd; // Temporary buffer for text input
    uint16_t temp_buffer_ha_path_cmd_size; // Size of temporary buffer

    UART_TextInput* text_input_ha_ssid; // The text input screen
    VariableItem* ha_ssid_item; // The name setting item (so we can update the text)
    char* temp_buffer_ha_ssid; // Temporary buffer for text input
    uint16_t temp_buffer_ha_ssid_size; // Size of temporary buffer
    FuriString* ha_ssid;

    UART_TextInput* text_input_ha_pass; // The text input screen
    VariableItem* pass_ha_item; // The name setting item (so we can update the text)
    char* temp_buffer_ha_pass; // Temporary buffer for text input
    uint16_t temp_buffer_ha_pass_size; // Size of temporary buffer
    FuriString* ha_pass;

    VariableItem* polling_ha_item; // The name setting item (so we can update the text)
    FuriMutex* config_mutex;

    FuriTimer* timer_draw; // Timer for redrawing the screen

    FuriTimer* timer_reset_key;
    FuriThreadId comm_thread_id;
    FuriThread* comm_thread;
    FuriTimer* timer_comm_upd;
} App;

typedef struct {
    bool req_sts;
    bool populated;
    FuriString* url;
    FuriString* url_cmd;
    FuriString* headers;
    FuriString* payload;
    FuriString* payload_dehum;
    FuriString* req_path;
    FuriString* token;
    uint16_t token_lenght;
    FuriMutex* worker_mutex;
    InputKey last_input;
    FuriString* curr_cmd;
    uint16_t polling_rate;
    uint16_t polling_rate_index;

    FuriString* print_bedroom_temp;
    FuriString* print_bedroom_hum;
    FuriString* print_kitchen_temp;
    FuriString* print_kitchen_hum;
    FuriString* print_outside_temp;
    FuriString* print_outside_hum;
    FuriString* print_dehum_sts;
    FuriString* print_co2;
    FuriString* print_pm2_5;
    FuriString* print_aidx;
    int8_t curr_page;
} ReqModel;

void extract_payload(
    const char* string,
    FuriString* dest,
    const char* beginning,
    const char* ending);

void save_settings(App* app);
void load_settings(App* app);

void buzz_vibration();

void polling_setting_changed(VariableItem* item);
void conf_text_updated(void* context);

uint32_t navigation_configure_callback(void* context);
void submenu_callback(void* context, uint32_t index);
uint32_t navigation_exit_callback(void* context);
void setting_item_clicked(void* context, uint32_t index);
uint32_t navigation_submenu_callback(void* context);
bool allow_cmd(ReqModel* model);

void reverse_array_uint8(uint8_t* arr, size_t size);

void text_box_format_msg(App* app, const char* message, TextBox* text_box);

void view_timer_key_reset_callback(void* context);

bool view_custom_event_callback(uint32_t event, void* context);

void comm_thread_timer_callback(void* context);

void view_resp_exit_callback(void* context);
