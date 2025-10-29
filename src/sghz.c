#include "sghz.h"

/**
 * @brief      Subghz data ready callback
 * @param      context SghzComm pointer
*/
void subghz_worker_update_rx(void* context) {
    furi_assert(context);
    App* app = context;
    ReqModel* ha_model = view_get_model(app->view_ha);
    notification_message(app->notification, &sequence_blink_blue_10);
    furi_thread_flags_set(ha_model->sghz->rx_thread_id, ThreadCommUpdData);
}

/**
 * @brief      Subghz data reading thread
 * @param      context ReqModel pointer
*/
int32_t listen_rx(void* context) {
    furi_assert(context);
    ReqModel* ha_model = context;
    FURI_LOG_I(TAG, "listen_rx started...");
    size_t message_max_len = 64;
    uint8_t message[64] = {0};
    FuriString* output = furi_string_alloc();
    bool run = true;
    while(run) {
        uint32_t events = furi_thread_flags_wait(
            ThreadCommUpdData | ThreadCommStop, FuriFlagWaitAny, FuriWaitForever);
        switch(events) {
        case ThreadCommUpdData:
            while(subghz_tx_rx_worker_available(ha_model->sghz->subghz_txrx)) {
                ha_model->sghz->status = SGHZ_BUSY;
                memset(message, 0x00, message_max_len);
                size_t len = subghz_tx_rx_worker_read(
                    ha_model->sghz->subghz_txrx, message, message_max_len);
                for(size_t i = 0; i < len; i++) {
                    furi_string_push_back(output, message[i]);
                }
                FURI_LOG_I(SGHZ_TAG, "[Message] %s", message);
            }

            if(furi_mutex_acquire(ha_model->worker_mutex, 0) == FuriStatusOk) {
                furi_string_set(ha_model->sghz->last_message, output);
                ha_model->sghz->status = SGHZ_INACTIVE;
                furi_check(furi_mutex_release(ha_model->worker_mutex) == FuriStatusOk);
            }

            furi_string_reset(output);

            break;

        case ThreadCommStop:
            run = false;
            ha_model->sghz->status = SGHZ_INACTIVE;
            break;

        default:
            break;
        }
    }

    furi_string_free(output);
    return 0;
}
