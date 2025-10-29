#pragma once
#include <notification/notification_messages.h>
#include "devices/devices.h"
#include "home_remote_icons.h"
#include <libs/furi_utils.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <libs/easy_flipper.h>
#include <libs/flipper_http.h>

#define TAG                 "HOME_REMOTE"
#define HR_APPS_DATA_FOLDER EXT_PATH("apps_data")
#define HR_SETTINGS_FOLDER  \
    HR_APPS_DATA_FOLDER "/" \
                        "home_remote"
#define HR_CONF_FILE_NAME "conf.json"
#define HR_CONF_PATH      HR_SETTINGS_FOLDER "/" HR_CONF_FILE_NAME

#define INPUT_RESET      0xFF
#define DRAW_PERIOD      100U
#define RESET_KEY_PERIOD 200U

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
    ConfigVariableItemPolling,
    ConfigVariableItemCtrlMode,
    ConfigVariableItemRandomizeMac,
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
    EventIdSGHzRxDone = 51,
} EventId;

typedef enum {
    PROCESSING_INACTIVE, // Inactive state
    PROCESSING_BUSY, // Receiving data
    PROCESSING_DONE, // Receiving data done
} ProccessingState;

typedef enum {
    BEACON_INACTIVE,
    BEACON_BUSY,
} BeaconStatus;

typedef enum {
    BtStateChecking,
    BtStateInactive,
    BtStateWaiting,
    BtStateRecieving,
    BtStateNoData,
    BtStateLost
} BtState;

typedef enum {
    SGHZ_INIT,
    SGHZ_INACTIVE,
    SGHZ_BUSY,
} SghzStatus;

typedef enum {
    DUDUBUBU_FIRST = 1,
    DUDUBUBU_BUTT = 1,
    DUDUBUBU_HUG = 2,
    DUDUBUBU_LAST = 2,
} AboutPic;

typedef enum {
    ThreadCommStop = 0b00000001,
    ThreadCommUpdData = 0b00000010,
    ThreadCommSendCmd = 0b00000100,
    ThreadCommStopCmd = 0b00001000,
    ThreadCommSendCmdBt = 0b00010000,
} EventCommReq;

typedef enum {
    PageFirst,
    PageSecond,
    PageThird,
    PageLast,
} PageIndex;

typedef enum {
    HaCtrlWifi,
    HaCtrlSghzBtHome,
    HaCtrlBtSerial
} HaCtrlMode;

typedef struct App {
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher; // Switches between our views
    Submenu* submenu; // The application menu
    VariableItemList* variable_item_list_config; // The configuration screen
    View* view_frame;
    View* view_ha;
    uint8_t current_view;
    TextBox* text_box_resp;
    Widget* widget_about; // The about screen

    char* formatted_message;

    uint8_t config_index; //Index for what text input changed
    char* temp_buffer; // Temporary buffer for text input - common

    UART_TextInput* text_input_frame_path; // The text input screen
    VariableItem* frame_path_item;
    char* temp_buffer_frame_path; // Temporary buffer for text input
    uint16_t temp_buffer_frame_path_size; // Size of temporary buffer

    UART_TextInput* text_input_frame_ssid; // The text input screen
    VariableItem* frame_ssid_item;
    char* temp_buffer_frame_ssid; // Temporary buffer for text input
    uint16_t temp_buffer_frame_ssid_size; // Size of temporary buffer
    FuriString* frame_ssid;

    UART_TextInput* text_input_frame_pass; // The text input screen
    VariableItem* pass_frame_item;
    char* temp_buffer_frame_pass; // Temporary buffer for text input
    uint16_t temp_buffer_frame_pass_size; // Size of temporary buffer
    FuriString* frame_pass;

    UART_TextInput* text_input_ha_path; // The text input screen
    VariableItem* ha_path_item;
    char* temp_buffer_ha_path; // Temporary buffer for text input
    uint16_t temp_buffer_ha_path_size; // Size of temporary buffer

    UART_TextInput* text_input_ha_path_cmd; // The text input screen
    VariableItem* ha_path_cmd_item;
    char* temp_buffer_ha_path_cmd; // Temporary buffer for text input
    uint16_t temp_buffer_ha_path_cmd_size; // Size of temporary buffer

    UART_TextInput* text_input_ha_ssid; // The text input screen
    VariableItem* ha_ssid_item;
    char* temp_buffer_ha_ssid; // Temporary buffer for text input
    uint16_t temp_buffer_ha_ssid_size; // Size of temporary buffer
    FuriString* ha_ssid;

    UART_TextInput* text_input_ha_pass; // The text input screen
    VariableItem* pass_ha_item;
    char* temp_buffer_ha_pass; // Temporary buffer for text input
    uint16_t temp_buffer_ha_pass_size; // Size of temporary buffer
    FuriString* ha_pass;

    VariableItem* polling_ha_item;
    VariableItem* ctrl_mode_ha_item;
    VariableItem* randomize_mac_enb_item;
    uint8_t bool_config_index;
    FuriMutex* config_mutex;

    FuriTimer* timer_draw; // Timer for redrawing the screen

    FuriTimer* timer_reset_key;
    FuriThreadId comm_thread_id;
    FuriThread* comm_thread;
    FuriTimer* timer_comm_upd;
} App;

typedef struct {
    FuriMutex* worker_mutex;
    InputKey last_input;
    uint8_t status;
    SubGhzTxRxWorker* subghz_txrx;
    const SubGhzDevice* device;
    FuriThread* rx_thread;
    FuriThreadId rx_thread_id;
    FuriString* last_message;
    int8_t curr_page;
    uint8_t last_counter;
} SghzComm;

typedef struct {
    FuriString* mac_address_str;
    uint8_t status;
    FuriTimer* timer_reset_beacon;
    const char* default_device_name;
    uint8_t default_name_len;
    // Previous Beacon
    GapExtraBeaconConfig prev_config;
    uint8_t prev_data[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t prev_data_len;
    bool prev_active;
    bool prev_exists;
    // BR Home Data
    uint8_t cnt;
    char* device_name;
    size_t device_name_len;
    int8_t curr_page;
    uint8_t event_type;
    // Beacon settings
    GapExtraBeaconConfig config;
    uint16_t beacon_period;
    uint16_t beacon_duration;
    uint8_t beacon_period_idx;
    uint8_t beacon_duration_idx;
    bool randomize_mac_enb;
} BtBeacon;

#pragma pack(push, 1)
typedef struct {
    float_t bedroom_temp;
    float_t bedroom_hum;
    float_t kitchen_temp;
    float_t kitchen_hum;
    float_t outside_temp;
    float_t outside_hum;
    uint8_t dehum_sts;
    uint8_t dehum_aut_sts;
    uint16_t co2;
    uint16_t pm2_5;
} DataStruct;
#pragma pack(pop)

typedef struct {
    Bt* bt;
    FuriString* mac_address_str;
    uint8_t status;
    FuriHalBleProfileBase* ble_serial_profile;
    BtState bt_state;
    DataStruct data;
    uint8_t lines_count;
    uint32_t last_packet;
} BtSerial;

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
    uint8_t control_mode;
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
    BtBeacon* ble;
    SghzComm* sghz;
    BtSerial* bt_serial;
} ReqModel;

void save_settings(App* app);
void load_settings(App* app);
void variable_item_setting_changed(VariableItem* item);
void conf_text_updated(void* context);
uint32_t navigation_configure_callback(void* context);
void submenu_callback(void* context, uint32_t index);
uint32_t navigation_exit_callback(void* context);
void setting_item_clicked(void* context, uint32_t index);
uint32_t navigation_submenu_callback(void* context);
bool allow_cmd(ReqModel* model);
void view_timer_key_reset_callback(void* context);
bool view_custom_event_callback(uint32_t event, void* context);
void comm_thread_timer_callback(void* context);
void view_resp_exit_callback(void* context);
