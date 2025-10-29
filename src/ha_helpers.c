#include "ha_helpers.h"

static const char HA_BEDROOM_TEMP_KEY[] = "bt";
static const char HA_BEDROOM_HUM_KEY[] = "bh";
static const char HA_KITCHEN_TEMP_KEY[] = "kt";
static const char HA_KITCHEN_HUM_KEY[] = "kh";
static const char HA_OUTSIDE_TEMP_KEY[] = "ot";
static const char HA_OUTSIDE_HUM_KEY[] = "oh";
static const char HA_DEHUM_KEY[] = "dh";
static const char HA_DEHUM_AUTOMATION_KEY[] = "ad";
static const char HA_CO2_KEY[] = "co";
static const char HA_PM2_5_KEY[] = "pm";

void parse_ha_json(const char* response, ReqModel* ha_model) {
    const uint16_t max_tokens = 128;
    // Must free the return value memory
    char* value;
    value = get_json_value(HA_BEDROOM_TEMP_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_bedroom_temp, value);
        free(value);
    }
    value = get_json_value(HA_BEDROOM_HUM_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_bedroom_hum, value);
        free(value);
    }

    value = get_json_value(HA_KITCHEN_TEMP_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_kitchen_temp, value);
        free(value);
    }
    value = get_json_value(HA_KITCHEN_HUM_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_kitchen_hum, value);
        free(value);
    }

    value = get_json_value(HA_OUTSIDE_TEMP_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_outside_temp, value);
        free(value);
    }
    value = get_json_value(HA_OUTSIDE_HUM_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_outside_hum, value);
        free(value);
    }

    value = get_json_value(HA_DEHUM_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_dehum_sts, value);
        free(value);
    }

    value = get_json_value(HA_DEHUM_AUTOMATION_KEY, response, max_tokens);
    if(value) {
        if(strstr(value, "on")) {
            furi_string_cat_str(ha_model->print_dehum_sts, "-A");
        } else {
            furi_string_cat_str(ha_model->print_dehum_sts, "-M");
        }
    }

    value = get_json_value(HA_CO2_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_co2, value);
        free(value);
    }

    value = get_json_value(HA_PM2_5_KEY, response, max_tokens);
    if(value) {
        furi_string_set_str(ha_model->print_pm2_5, value);
        free(value);
    }
}

void parse_ha_sghz(const char* string, ReqModel* ha_model) {
    const size_t FIELD_SIZE = 4;
    char counter[] = "00";
    memcpy(counter, string, 2);
    counter[sizeof(counter) - 1] = '\0';
    uint8_t counter_u = strtoul(counter, NULL, 0);

    if(counter_u != ha_model->sghz->last_counter &&
       furi_string_size(ha_model->sghz->last_message) > 0) {
        ha_model->sghz->last_counter = counter_u;
        char temp_str[5];
        size_t pos = 0;
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_BEDROOM_TEMP_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            furi_string_set_str(ha_model->print_bedroom_temp, temp_str);
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_BEDROOM_HUM_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            furi_string_set_str(ha_model->print_bedroom_hum, temp_str);
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_KITCHEN_TEMP_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            furi_string_set_str(ha_model->print_kitchen_temp, temp_str);
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_KITCHEN_HUM_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            furi_string_set_str(ha_model->print_kitchen_hum, temp_str);
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_OUTSIDE_TEMP_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            furi_string_set_str(ha_model->print_outside_temp, temp_str);
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_OUTSIDE_HUM_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            furi_string_set_str(ha_model->print_outside_hum, temp_str);
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_CO2_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            furi_string_set_str(ha_model->print_co2, temp_str);
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_PM2_5_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            furi_string_set_str(ha_model->print_pm2_5, temp_str);
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_DEHUM_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            if(strstr(temp_str, "on")) {
                furi_string_set_str(ha_model->print_dehum_sts, "on");
            } else {
                furi_string_set_str(ha_model->print_dehum_sts, "off");
            }
        }
        pos = furi_string_search_str(ha_model->sghz->last_message, HA_DEHUM_AUTOMATION_KEY, 0);
        if(pos != FURI_STRING_FAILURE) {
            memcpy(temp_str, &string[pos + 2], FIELD_SIZE);
            temp_str[sizeof(temp_str) - 1] = '\0';
            if(strstr(temp_str, "on")) {
                furi_string_cat_str(ha_model->print_dehum_sts, "-A");
            } else {
                furi_string_cat_str(ha_model->print_dehum_sts, "-M");
            }
        }
    }
}

void parse_ha_bt_serial(DataStruct* data, ReqModel* ha_model) {
    char temp_str[6];
    snprintf(temp_str, sizeof(temp_str) - 1, "%2.2f", (double_t)data->bedroom_temp);
    furi_string_set_str(ha_model->print_bedroom_temp, temp_str);
    snprintf(temp_str, sizeof(temp_str) - 1, "%2.2f", (double_t)data->bedroom_hum);
    furi_string_set_str(ha_model->print_bedroom_hum, temp_str);

    snprintf(temp_str, sizeof(temp_str) - 1, "%2.2f", (double_t)data->kitchen_temp);
    furi_string_set_str(ha_model->print_kitchen_temp, temp_str);
    snprintf(temp_str, sizeof(temp_str) - 1, "%2.2f", (double_t)data->kitchen_hum);
    furi_string_set_str(ha_model->print_kitchen_hum, temp_str);

    snprintf(temp_str, sizeof(temp_str) - 1, "%2.2f", (double_t)data->outside_temp);
    furi_string_set_str(ha_model->print_outside_temp, temp_str);
    snprintf(temp_str, sizeof(temp_str) - 1, "%2.2f", (double_t)data->outside_hum);
    furi_string_set_str(ha_model->print_outside_hum, temp_str);

    if(data->dehum_sts) {
        furi_string_set_str(ha_model->print_dehum_sts, "on");
    } else {
        furi_string_set_str(ha_model->print_dehum_sts, "off");
    }
    if(data->dehum_aut_sts) {
        furi_string_cat_str(ha_model->print_dehum_sts, "-A");
    } else {
        furi_string_cat_str(ha_model->print_dehum_sts, "-M");
    }

    snprintf(temp_str, sizeof(temp_str), "%u", data->co2);
    furi_string_set_str(ha_model->print_co2, temp_str);
    snprintf(temp_str, sizeof(temp_str), "%u", data->pm2_5);
    furi_string_set_str(ha_model->print_pm2_5, temp_str);
}
