/**
 *
 * Microvisor IoT Device Demo
 * Version 2.1.2
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


// Central store for Microvisor resource handles used in this code.
// See 'https://www.twilio.com/docs/iot/microvisor/syscalls#handles'
struct {
    MvNotificationHandle notification;
    MvNetworkHandle      network;
    uint32_t             log;
} net_handles = { 0, 0, 0 };

// Central store for notification records. Holds one record at
// a time -- each record is 16 bytes in size.
static volatile struct MvNotification net_notification_buffer[16];

const uint32_t log_buffer_size = 4096;
static uint8_t log_buffer[4096] __attribute__((aligned(512))) = {0} ;


extern      UART_HandleTypeDef   uart;



/**
 * @brief  Open a logging channel.
 *
 * Open a data channel for Microvisor logging.
 * This call will also request a network connection.
 */
void log_start(void) {
    // Initiate the Microvisor logging service
    log_service_setup();
    
    // Connect to the network
    // NOTE This connection spans logging and HTTP comms
    net_open_network();
}


/**
 * @brief Configure and connect to the network.
 */
void net_open_network() {
    // Configure the network's notification center
    net_notification_center_setup();
    
    if (net_handles.network == 0) {
        // Configure the network connection request
        struct MvRequestNetworkParams network_config = {
            .version = 1,
            .v1 = {
                .notification_handle = net_handles.notification,
                .notification_tag = USER_TAG_LOGGING_REQUEST_NETWORK,
            }
        };

        // Ask Microvisor to establish the network connection
        // and confirm that it has accepted the request
        enum MvStatus status = mvRequestNetwork(&network_config, &net_handles.network);
        assert(status == MV_STATUS_OKAY);

        // The network connection is established by Microvisor asynchronously,
        // so we wait for it to come up before opening the data channel -- which
        // would fail otherwise
        enum MvNetworkStatus net_status;
        while (1) {
            // Request the status of the network connection, identified by its handle.
            // If we're good to continue, break out of the loop...
            if (mvGetNetworkStatus(net_handles.network, &net_status) == MV_STATUS_OKAY && net_status == MV_NETWORKSTATUS_CONNECTED) {
                break;
            }

            // ... or wait a short period before retrying
            for (volatile unsigned i = 0; i < 50000; ++i) {
                // No op
                __asm("nop");
            }
        }
    }
}


/**
 * @brief Configure the network Notification Center.
 */
void net_notification_center_setup() {
    if (net_handles.notification == 0) {
        // Clear the notification store
        memset((void *)net_notification_buffer, 0xff, sizeof(net_notification_buffer));

        // Configure a notification center for network-centric notifications
        static struct MvNotificationSetup net_notification_config = {
            .irq = TIM1_BRK_IRQn,
            .buffer = (struct MvNotification *)net_notification_buffer,
            .buffer_size = sizeof(net_notification_buffer)
        };

        // Ask Microvisor to establish the notification center
        // and confirm that it has accepted the request
        enum MvStatus status = mvSetupNotifications(&net_notification_config, &net_handles.notification);
        assert(status == MV_STATUS_OKAY);

        // Start the notification IRQ
        NVIC_ClearPendingIRQ(TIM1_BRK_IRQn);
        NVIC_EnableIRQ(TIM1_BRK_IRQn);
    }
}


/**
 * @brief Initiate Microvisor application logging.
 */
void log_service_setup(void) {
    if (net_handles.log == 0) {
        // Initialse logging with the standard system call
        enum MvStatus status = mvServerLoggingInit(log_buffer, log_buffer_size);
        
        // Set a mock handle as a proxy for a 'logging enabled' flag
        if (status == MV_STATUS_OKAY) net_handles.log = USER_HANDLE_LOGGING_STARTED;
        assert(status == MV_STATUS_OKAY);
    }
}


/**
 * @brief Issue a debug message.
 *
 * @param format_string Message string with optional formatting
 * @param ...           Optional injectable values
 */
void server_log(char* format_string, ...) {
    if (LOG_DEBUG_MESSAGES) {
        va_list args;
        va_start(args, format_string);
        do_log(false, format_string, args);
        va_end(args);
    }
}


/**
 * @brief Issue an error message.
 *
 * @param format_string Message string with optional formatting
 * @param ...           Optional injectable values
 */
void server_error(char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    do_log(false, format_string, args);
    va_end(args);
}


/**
 * @brief Issue any log message.
 *
 * @param is_err        Is the message an error?
 * @param format_string Message string with optional formatting
 * @param args          va_list of args from previous call
 */
void do_log(bool is_err, char* format_string, va_list args) {
    if (get_net_handle() == 0) log_start();
    char buffer[1024] = {0};
    
    // Add a timestamp
    // NO LONG REQUIRED WITH MV PLUGIN 0.3.3 :-(
    /*
    char timestamp[64] = {0};
    uint64_t usec = 0;
    time_t sec = 0;
    time_t msec = 0;
    enum MvStatus status = mvGetWallTime(&usec);
    if (status == MV_STATUS_OKAY) {
        // Get the second and millisecond times
        sec = (time_t)usec / 1000000;
        msec = (time_t)usec / 1000;
    }
    
    // Write time string as "2022-05-10 13:30:58.XXX "
    strftime(timestamp, 64, "%F %T.XXX ", gmtime(&sec));

    // Insert the millisecond time over the XXX
    sprintf(&timestamp[20], "%03u", (unsigned)(msec % 1000));
    
    // Write the timestamp to the message
    strcpy(buffer, timestamp);
    size_t len = strlen(timestamp);
    */
    
    // Write the message type to the message
    sprintf(buffer, is_err ? "[ERROR] " : "[DEBUG] ");
    
    // Write the formatted text to the message
    vsnprintf(&buffer[8], sizeof(buffer) - 9, format_string, args);
    
    // Output the message using the system call
    mvServerLog((const uint8_t*)buffer, (uint16_t)strlen(buffer));
}


/**
 *  @brief Provide the current network handle.
 */
MvNetworkHandle get_net_handle(void) {
    return net_handles.network;
}


/**
 *  @brief Provide the current logging handle.
 */
uint32_t get_log_handle(void) {
    return net_handles.log;
}


/**
 *  @brief Network notification ISR.
 */
void TIM1_BRK_IRQHandler(void) {
    // Netwokrk notifications interrupt service handler
    // Add your own notification processing code here
}
