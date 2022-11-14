/**
 *
 * Microvisor IoT Device Demo
 * Version 2.1.4
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


/*
 * STATIC PROTOTYPES
 */
static void net_open_network(void);
static void net_notification_center_setup(void);
static void log_start(void);
static void log_service_setup(void);


/*
 * GLOBALS
 */
// Central store for Microvisor resource handles used in this code.
// See 'https://www.twilio.com/docs/iot/microvisor/syscalls#handles'
struct {
    MvNotificationHandle notification;
    MvNetworkHandle      network;
    uint32_t             log;
} net_handles = { 0, 0, 0 };

// Central store for network management notification records.
// Holds eight records at a time -- each record is 16 bytes in size.
static volatile struct MvNotification net_notification_buffer[8] __attribute__((aligned(8)));

const uint32_t log_buffer_size = 4096;
static uint8_t log_buffer[4096] __attribute__((aligned(512))) = {0} ;


extern      UART_HandleTypeDef   uart;



/**
 * @brief  Open a logging channel.
 *
 * Open a data channel for Microvisor logging.
 * This call will also request a network connection.
 */
static void log_start(void) {
    // Initiate the Microvisor logging service
    log_service_setup();

    // Connect to the network
    // NOTE This connection spans logging and HTTP comms
    net_open_network();
}


/**
 * @brief Configure and connect to the network.
 */
static void net_open_network() {
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
 *
 * @brief  Open the logging channel.
 *
 * Close the data channel -- and the network connection -- when
 * we're done with it.
 */
void log_close_channel(void) {
    enum MvStatus status;

    // If we have a valid channel handle -- ie. it is non-zero --
    // then ask Microvisor to close it and confirm acceptance of
    // the closure request.
    if (log_handles.channel != 0) {
        status = mvCloseChannel(&log_handles.channel);
        if (status != MV_STATUS_OKAY) report_and_assert(ERR_LOG_CHANNEL_NOT_CLOSED);
    }

    // Confirm the channel handle has been invalidated by Microvisor
    if (log_handles.channel != 0) report_and_assert(ERR_LOG_CHANNEL_HANDLE_NOT_ZERO);

    // If we have a valid network handle, then ask Microvisor to
    // close the connection and confirm acceptance of the request.
    if (log_handles.network != 0) {
        status = mvReleaseNetwork(&log_handles.network);
        if (status != MV_STATUS_OKAY) report_and_assert(ERR_NETWORK_NOT_CLOSED);
    }

    // Confirm the network handle has been invalidated by Microvisor
    if (log_handles.network != 0) report_and_assert(ERR_NETWORK_HANDLE_NOT_ZERO);

    // If we have a valid notification center handle, then ask Microvisor
    // to tear down the center and confirm acceptance of the request.
    if (log_handles.notification != 0) {
        status = mvCloseNotifications(&log_handles.notification);
        if (status != MV_STATUS_OKAY) report_and_assert(ERR_NOTIFICATION_CENTER_NOT_CLOSED);
    }

    // Confirm the notification center handle has been invalidated by Microvisor
    if (log_handles.notification != 0) report_and_assert(ERR_NOTIFICATION_CENTER_HANDLE_NOT_ZERO);

    NVIC_DisableIRQ(TIM1_BRK_IRQn);
    NVIC_ClearPendingIRQ(TIM1_BRK_IRQn);
}


/**
 *
 * @brief Wire up the `stdio` system call, so that `printf()`
 *        works as a logging message generator.
 * @param  file    The log entry -- a C string -- to send.
 * @param  ptr     A pointer to the C string we want to send.
 * @param  length  The length of the message.
 *
 * @return         The number of bytes written, or -1 to indicate error.
 */
int _write(int file, char *ptr, int length) {
    if (file != STDOUT_FILENO) {
        errno = EBADF;
        return -1;
    }

    // Do we have an open channel? If not, any stored channel handle
    // will be invalid, ie. zero. If that's the case, open a channel
    if (log_handles.channel == 0) {
        log_open_channel();
    }

    // Prepare and add a timestamp if we can
    // (if we can't, we show no time)
    char timestamp[64] = {0};
    uint64_t usec = 0;
    enum MvStatus status = mvGetWallTime(&usec);
    if (status == MV_STATUS_OKAY) {
        // Get the second and millisecond times
        time_t sec = (time_t)usec / 1000000;
        time_t msec = (time_t)usec / 1000;

        // Write time string as "2022-05-10 13:30:58.XXX "
        strftime(timestamp, 64, "%F %T.XXX ", gmtime(&sec));

        // Insert the millisecond time over the XXX
        sprintf(&timestamp[20], "%03u ", (unsigned)(msec % 1000));
    }

    // Write out the time string. Confirm that Microvisor
    // has accepted the request to write data to the channel.
    uint32_t time_chars = 0;
    size_t len = strlen(timestamp);
    if (len > 0) {
        status = mvWriteChannelStream(log_handles.channel, (const uint8_t*)timestamp, len, &time_chars);

        // FROM 1.3.3 -- Reset the log channel on closure
        if (status == MV_STATUS_CHANNELCLOSED) {
            log_close_channel();
            log_open_channel();
            status = mvWriteChannelStream(log_handles.channel, (const uint8_t*)timestamp, len, &time_chars);
        }

        if (status != MV_STATUS_OKAY) {
            errno = EIO;
            return -1;
        }
    }

    // Write out the message string. Confirm that Microvisor
    // has accepted the request to write data to the channel.
    uint32_t msg_chars = 0;
    status = mvWriteChannelStream(log_handles.channel, (const uint8_t*)ptr, length, &msg_chars);

    // FROM 1.3.3 -- Reset the log channel on closure
    if (status == MV_STATUS_CHANNELCLOSED) {
        log_close_channel();
        log_open_channel();
        status = mvWriteChannelStream(log_handles.channel, (const uint8_t*)ptr, length, &msg_chars);
    }

    if (status == MV_STATUS_OKAY) {
        // Return the number of characters written to the channel
        return time_chars + msg_chars;
    } else {
        errno = EIO;
        return -1;
    }
}


/**
 * @brief Configure the logging channel Notification Center.
 */
void log_channel_center_setup() {
    if (log_handles.notification == 0) {
        // Clear the notification store
        memset((void *)log_notification_buffer, 0xff, sizeof(log_notification_buffer));

        // Configure a notification center for network-centric notifications
        static struct MvNotificationSetup log_notification_config = {
            .irq = TIM1_BRK_IRQn,
            .buffer = (struct MvNotification *)log_notification_buffer,
            .buffer_size = sizeof(log_notification_buffer)
        };

        // Ask Microvisor to establish the notification center
        // and confirm that it has accepted the request
        enum MvStatus status = mvSetupNotifications(&log_notification_config, &log_handles.notification);
        if (status != MV_STATUS_OKAY) report_and_assert(ERR_NETWORK_NC_NOT_OPEN);

        // Start the notification IRQ
        NVIC_ClearPendingIRQ(TIM1_BRK_IRQn);
        NVIC_EnableIRQ(TIM1_BRK_IRQn);
    }
}


/**
 * @brief Configure and connect to the network.
 */
void log_open_network() {
    if (log_handles.network == 0) {
        // Configure the network connection request
        struct MvRequestNetworkParams network_config = {
            .version = 1,
            .v1 = {
                .notification_handle = log_handles.notification,
                .notification_tag = USER_TAG_LOGGING_REQUEST_NETWORK,
            }
        };

        // Ask Microvisor to establish the network connection
        // and confirm that it has accepted the request
        enum MvStatus status = mvRequestNetwork(&network_config, &log_handles.network);
        if (status != MV_STATUS_OKAY) report_and_assert(ERR_NETWORK_NOT_OPEN);

        // The network connection is established by Microvisor asynchronously,
        // so we wait for it to come up before opening the data channel -- which
        // would fail otherwise
        enum MvNetworkStatus net_status;
        while (true) {
            // Request the status of the network connection, identified by its handle.
            // If we're good to continue, break out of the loop...
            if (mvGetNetworkStatus(log_handles.network, &net_status) == MV_STATUS_OKAY && net_status == MV_NETWORKSTATUS_CONNECTED) {
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
 *  @brief Provide the current network handle.
 */
MvNetworkHandle get_net_handle() {
    return log_handles.network;
}


/**
 *  @brief Network notification ISR.
 */
void TIM1_BRK_IRQHandler(void) {

}
