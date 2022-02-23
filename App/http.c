/**
 *
 * Microvisor IoT Device Demo
 * Version 1.0.2
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


// Central store for Microvisor resource handles used in this code.
// See `https://www.twilio.com/docs/iot/microvisor/syscalls#http_handles`
struct {
    MvNotificationHandle notification;
    MvNetworkHandle      network;
    MvChannelHandle      channel;
} http_handles = { 0, 0, 0 };

// Central store for notification records.
// Holds one record at a time -- each record is 16 bytes.
volatile struct MvNotification http_notification_center[16];

// Defined in `main.c`
extern volatile bool request_recv;


/**
 *  @brief Open a new HTTP channel.
 */
void http_open_channel(void) {
    // Set up the HTTP channel's multi-use send and receive buffers
    static volatile uint8_t http_rx_buffer[1536] __attribute__((aligned(512)));
    static volatile uint8_t http_tx_buffer[512] __attribute__((aligned(512)));
    static const char endpoint[] = "";

    // Get the network channel handle.
    // NOTE This is set in `logging.c` which puts the network in place
    //      (ie. so the network handle != 0) well in advance of this being called
    http_handles.network = get_net_handle();
    assert((http_handles.network != 0) && "[ERROR] Network handle not non-zero");
    printf("[DEBUG] Network handle: %lu\n", (uint32_t)http_handles.network);

    // Configure the required data channel
    struct MvOpenChannelParams channel_config = {
        .version = 1,
        .v1 = {
            .notification_handle = http_handles.notification,
            .notification_tag    = USER_TAG_HTTP_OPEN_CHANNEL,
            .network_handle      = http_handles.network,
            .receive_buffer      = (uint8_t*)http_rx_buffer,
            .receive_buffer_len  = sizeof(http_rx_buffer),
            .send_buffer         = (uint8_t*)http_tx_buffer,
            .send_buffer_len     = sizeof(http_tx_buffer),
            .channel_type        = MV_CHANNELTYPE_HTTP,
            .endpoint            = (uint8_t*)endpoint,
            .endpoint_len        = 0
        }
    };

    // Ask Microvisor to open the channel
    // and confirm that it has accepted the request
    uint32_t status = mvOpenChannel(&channel_config, &http_handles.channel);
    if (status == MV_STATUS_OKAY) {
        printf("[DEBUG] HTTP channel open. Handle: %lu\n", (uint32_t)http_handles.channel);
        assert((http_handles.channel != 0) && "[ERROR] Channel handle not non-zero");
    } else {
        printf("[ERROR] HTTP channel closed. Status: %lu\n", status);
    }
}


/**
 *  @brief Close the currently open HTTP channel.
 */
void http_close_channel(void) {
    // If we have a valid channel handle -- ie. it is non-zero --
    // then ask Microvisor to close it and confirm acceptance of
    // the closure request.
    if (http_handles.channel != 0) {
        uint32_t status = mvCloseChannel(&http_handles.channel);
        printf("[DEBUG] HTTP channel closed\n");
        assert((status == MV_STATUS_OKAY || status == MV_STATUS_CHANNELCLOSED) && "[ERROR] Channel closure");
    }

    // Confirm the channel handle has been invalidated by Microvisor
    assert((http_handles.channel == 0) && "[ERROR] Channel handle not zero");
}


/**
 * @brief Configure the channel Notification Center.
 */
void http_channel_center_setup(void) {
    // Clear the notification store
    memset((void *)http_notification_center, 0xFF, sizeof(http_notification_center));

    // Configure a notification center for network-centric notifications
    static struct MvNotificationSetup http_notification_setup = {
        .irq = TIM8_BRK_IRQn,
        .buffer = (struct MvNotification *)http_notification_center,
        .buffer_size = sizeof(http_notification_center)
    };

    // Ask Microvisor to establish the notification center
    // and confirm that it has accepted the request
    uint32_t status = mvSetupNotifications(&http_notification_setup, &http_handles.notification);
    assert((status == MV_STATUS_OKAY) && "[ERROR] Could not set up HTTP channel NC");

    // Start the notification IRQ
    NVIC_ClearPendingIRQ(TIM8_BRK_IRQn);
    NVIC_EnableIRQ(TIM8_BRK_IRQn);
    printf("[DEBUG] Notification center handle: %lu\n", (uint32_t)http_handles.notification);
}


/**
 * @brief Send a stock HTTP request.
 *
 * @returns `true` if the request was accepted by Microvisor, otherwise `false`
 */
bool http_send_request(double temp) {
    // Check for a valid channel handle
    if (http_handles.channel != 0) {
        printf("[DEBUG] Sending HTTP request\n");

        char* base = malloc(40 * sizeof(char));
        sprintf(base, "{\"temp\":%.02f}", temp);
        char body[strlen(base)];
        strcpy(body, base);
        free(base);

        // Set up the request
        const char verb[] = "POST";
        const char uri[] = API_URL;

        // Add a header
        const char header_text[] = "Content-Type: application/json";
        struct MvHttpHeader header = {
            .data = (uint8_t *)header_text,
            .length = strlen(header_text)
        };

        struct MvHttpHeader headers[] = { header };
        struct MvHttpRequest request_config = {
            .method = (uint8_t *)verb,
            .method_len = strlen(verb),
            .url = (uint8_t *)uri,
            .url_len = strlen(uri),
            .num_headers = 1,
            .headers = headers,
            .body = (uint8_t *)body,
            .body_len = strlen(body),
            .timeout_ms = 10000
        };

        // Issue the request -- and check its status
        uint32_t status = mvSendHttpRequest(http_handles.channel, &request_config);
        if (status == MV_STATUS_OKAY) {
            printf("[DEBUG] Request sent to Twilio\n");
            return true;
        }

        // Report send failure
        printf("[ERROR] Could not issue request. Status: %lu\n", status);
        return false;
    }

    // There's no open channel, so open open one now and
    // try to send again
    http_open_channel();
    return http_send_request(temp);
}


/**
 * @brief Process HTTP response data.
 */
void http_process_response(void) {
    // We have received data via the active HTTP channel so establish
    // an `MvHttpResponseData` record to hold response metadata
    struct MvHttpResponseData resp_data;
    uint32_t status = mvReadHttpResponseData(http_handles.channel, &resp_data);
    if (status == MV_STATUS_OKAY) {
        // Check we successfully issued the request (`result` is OK) and
        // the request was successful (status code 200)
        if (resp_data.result == MV_HTTPRESULT_OK) {
            if (resp_data.status_code == 200) {
                // Set up a buffer that we'll get Microvisor to write
                // the response body into
                uint8_t buffer[resp_data.body_length + 1];
                memset((void *)buffer, 0x00, resp_data.body_length + 1);
                status = mvReadHttpResponseBody(http_handles.channel, 0, buffer, resp_data.body_length);
                if (status == MV_STATUS_OKAY) {
                    // Retrieved the body data successfully so log it
                    printf("[DEBUG] HTTP response header count: %lu\n", resp_data.num_headers);
                    printf("[DEBUG] HTTP response body length: %lu\n", resp_data.body_length);
                    printf("%s\n", buffer);
                } else {
                    printf("[ERROR] HTTP response body read status %lu\n", status);
                }
            } else {
                printf("[ERROR] HTTP status code: %lu\n", resp_data.status_code);
            }
        } else {
            printf("[ERROR] Request failed. Status: %lu\n", (uint32_t)resp_data.result);
        }
    } else {
        printf("[ERROR] Response data read failed. Status: %lu\n", status);
    }
}


/**
 *  @brief The HTTP channel notification interrupt handler.
 *
 *  This is called by Microvisor -- we need to check for key events
 *  and extract HTTP response data when it is available.
 */
void TIM8_BRK_IRQHandler(void) {
    // Get the event type
    enum MvEventType event_kind = http_notification_center->event_type;
    printf("[DEBUG] Channel notification center IRQ called for event: %u\n", event_kind);

    if (event_kind == MV_EVENTTYPE_CHANNELDATAREADABLE) {
        // Flag we need to access received data and to close the HTTP channel
        // when we're back in the main loop. This lets us exit the ISR quickly.
        // We should not make Microvisor System Calls in the ISR.
        request_recv = true;
    }
}
