/**
 *
 * Microvisor IoT Device Demo
 * Version 2.1.0
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
 *
 *  @returns `true` if the channel is open, otherwise `false`.
 */
bool http_open_channel(void) {
    // Set up the HTTP channel's multi-use send and receive buffers
    static volatile uint8_t http_rx_buffer[1536] __attribute__((aligned(512)));
    static volatile uint8_t http_tx_buffer[512] __attribute__((aligned(512)));
    const char endpoint[] = "";

    // Get the network channel handle.
    // NOTE This is set in `logging.c` which puts the network in place
    //      (ie. so the network handle != 0) well in advance of this being called
    http_handles.network = get_net_handle();
    if (http_handles.network == 0) return false;
    server_log("Network handle: %lu", (uint32_t)http_handles.network);

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
    enum MvStatus status = mvOpenChannel(&channel_config, &http_handles.channel);
    if (status == MV_STATUS_OKAY) {
        server_log("HTTP channel handle: %lu", (uint32_t)http_handles.channel);
        return true;
    } else {
        server_error("HTTP channel opening failed. Status: %i", status);
    }

    return false;
}


/**
 *  @brief Close the currently open HTTP channel.
 */
void http_close_channel(void) {
    // If we have a valid channel handle -- ie. it is non-zero --
    // then ask Microvisor to close it and confirm acceptance of
    // the closure request.
    if (http_handles.channel != 0) {
        enum MvStatus status = mvCloseChannel(&http_handles.channel);
        server_log("HTTP channel closed");
        if (status != MV_STATUS_OKAY && status != MV_STATUS_CHANNELCLOSED) report_and_assert(ERR_CHANNEL_NOT_CLOSED);
    }

    // Confirm the channel handle has been invalidated by Microvisor
    if (http_handles.channel != 0) report_and_assert(ERR_CHANNEL_HANDLE_NOT_ZERO);
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
    enum MvStatus status = mvSetupNotifications(&http_notification_setup, &http_handles.notification);
    if (status != MV_STATUS_OKAY) {
        report_and_assert(ERR_NOTIFICATION_CENTER_NOT_OPEN);
        return;
    }

    // Start the notification IRQ
    NVIC_ClearPendingIRQ(TIM8_BRK_IRQn);
    NVIC_EnableIRQ(TIM8_BRK_IRQn);
    server_log("Notification center handle: %lu", (uint32_t)http_handles.notification);
}

bool http_send_warning() {
    // Check for a valid channel handle
    if (http_handles.channel != 0) {
        server_log("Sending HTTP request");

        const char base[] = "{\"warning\":\"double tap detected\"}";

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
            .body = (uint8_t *)base,
            .body_len = strlen(base),
            .timeout_ms = 10000
        };

        // Issue the request -- and check its status
        enum MvStatus status = mvSendHttpRequest(http_handles.channel, &request_config);
        if (status == MV_STATUS_OKAY) {
            server_log("Request sent to Twilio");
            return true;
        }

        // Report send failure
        server_error("Could not issue request. Status: %i", status);
        return false;
    }

    // There's no open channel, so open open one now and
    // try to send again
    http_open_channel();
    return http_send_warning();
}
/**
 * @brief Send a stock HTTP request.
 *
 * @returns `true` if the request was accepted by Microvisor, otherwise `false`
 */
bool http_send_request(double temp) {
    // Check for a valid channel handle
    if (http_handles.channel != 0) {
        server_log("Sending HTTP request");

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
        enum MvStatus status = mvSendHttpRequest(http_handles.channel, &request_config);
        if (status == MV_STATUS_OKAY) {
            server_log("Request sent to Twilio");
            return true;
        }

        // Report send failure
        server_error("Could not issue request. Status: %i", status);
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
    enum MvStatus status = mvReadHttpResponseData(http_handles.channel, &resp_data);
    if (status == MV_STATUS_OKAY) {
        // Check we successfully issued the request (`result` is OK) and
        // the request was successful (status code 200)
        if (resp_data.result == MV_HTTPRESULT_OK) {
            if (resp_data.status_code == 200) {
                server_log("HTTP response header count: %lu", resp_data.num_headers);
                server_log("HTTP response body length: %lu", resp_data.body_length);
                
                // Set up a buffer that we'll get Microvisor to write
                // the response body into
                uint8_t buffer[resp_data.body_length + 1];
                memset((void *)buffer, 0x00, resp_data.body_length + 1);
                status = mvReadHttpResponseBody(http_handles.channel, 0, buffer, resp_data.body_length);
                if (status == MV_STATUS_OKAY) {
                    // Retrieved the body data successfully so log it
                    server_log("Message body:\n%s", buffer);
                } else {
                    server_error("HTTP response body read status %i", status);
                }
            } else {
                server_error("HTTP status code: %lu", resp_data.status_code);
            }
        } else {
            server_error("Request failed. Status: %i", resp_data.result);
        }
    } else {
        server_error("Response data read failed. Status: %i", status);
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

    if (event_kind == MV_EVENTTYPE_CHANNELDATAREADABLE) {
        // Flag we need to access received data and to close the HTTP channel
        // when we're back in the main loop. This lets us exit the ISR quickly.
        // We should not make Microvisor System Calls in the ISR.
        request_recv = true;
    }
}
