#include "app.h"

void parse_ha_json(const char* response, ReqModel* ha_model);
void parse_ha_sghz(const char* string, ReqModel* ha_model);
void parse_ha_bt_serial(DataStruct* data, ReqModel* ha_model);
