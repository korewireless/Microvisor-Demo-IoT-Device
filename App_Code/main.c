/**
 *
 * Microvisor IoT Device Demo
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


/**
 *  GLOBALS
 */


// This is the FreeRTOS thread task that flashed the USER LED
// and operates the display
osThreadId_t LEDTask;
const osThreadAttr_t led_task_attributes = {
    .name = "LEDTask",
    .stack_size = 512,
    .priority = (osPriority_t) osPriorityNormal
};

// This is the FreeRTOS thread task that reads the sensor
// and displays the temperature on the LED
osThreadId_t IOTTask;
const osThreadAttr_t iot_task_attributes = {
    .name = "IOTTask",
    .stack_size = 1024,
    .priority = (osPriority_t) osPriorityNormal
};


// I2C-related values
I2C_HandleTypeDef i2c;


// Central store for Microvisor resource handles used in this code.
// See `https://www.twilio.com/docs/iot/microvisor/syscalls#http_handles`
struct {
    MvNotificationHandle notification;
    MvNetworkHandle      network;
    MvChannelHandle      channel;
} http_handles = { 0, 0, 0 };


/**
 *  Theses variables may be changed by interrupt handler code,
 *  so we mark them as `volatile` to ensure compiler optimization
 *  doesn't render them immutable at runtime
 */
volatile bool show_count = false;
volatile bool request_sent = false;
volatile bool request_recv = false;
volatile bool got_sensor = false;
volatile bool use_i2c = false;
volatile uint8_t display_buffer[17] = { 0 };
volatile uint16_t counter = 0;
volatile double temp = 0.0;

// Central store for notification records.
// Holds one record at a time -- each record is 16 bytes.
volatile struct MvNotification http_notification_center[16];



/**
 * @brief  The application entry point.
 */
int main(void) {
    // Reset of all peripherals, Initializes the Flash interface and the Systick.
    HAL_Init();

    // Configure the system clock
    system_clock_config();

    // Initialize the peripherals
    GPIO_init();
    I2C_init();

    // Init scheduler
    osKernelInitialize();

    // Create the thread(s)
    IOTTask = osThreadNew(start_iot_task, NULL, &iot_task_attributes);
    LEDTask = osThreadNew(start_led_task, NULL, &led_task_attributes);

    // Start the scheduler
    osKernelStart();

    // We should never get here as control is now taken by the scheduler,
    // but just in case...
    while (true) {
        // NOP
    }
}


/**
  * @brief  Get the MV clock value
  * @retval The clock value
  */
uint32_t SECURE_SystemCoreClockUpdate() {
    uint32_t clock = 0;
    mvGetHClk(&clock);
    return clock;
}


/**
  * @brief  System clock configuration
  * @retval None
  */
void system_clock_config(void) {
    SystemCoreClockUpdate();
    HAL_InitTick(TICK_INT_PRIORITY);
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void error_handler(void) {
    printf("[ERROR] via Error_Handler()\n");
}


/**
  * @brief  GPIO Initialization Function
  * @retval None
  */
void GPIO_init(void) {
    // Enable GPIO port clock
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    // Configure GPIO pin output Level
    HAL_GPIO_WritePin(LED_GPIO_BANK, LED_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUTTON_GPIO_BANK, BUTTON_GPIO_PIN, GPIO_PIN_RESET);

    // Configure GPIO pin : PA5 - Pin under test
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin   = LED_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LED_GPIO_BANK, &GPIO_InitStruct);

    // Configure GPIO pin : PA5 - Pin under test
    GPIO_InitTypeDef GPIO_InitStruct2 = { 0 };
    GPIO_InitStruct2.Pin   = BUTTON_GPIO_PIN;
    GPIO_InitStruct2.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct2.Pull  = GPIO_PULLDOWN;
    HAL_GPIO_Init(BUTTON_GPIO_BANK, &GPIO_InitStruct2);
}


/**
  * @brief  Function implementing the GPIO Task thread.
  * @param  argument: Not used
  * @retval None
  */
void start_led_task(void *argument) {
    uint32_t last_tick = 0;

    // Button state values
    uint32_t press_debounce = 0;
    uint32_t release_debounce = 0;
    bool pressed = false;

    // Set up the display if it's available
    if (use_i2c) HT16K33_init();

    while (true) {
        // Get the ms timer value and read the button
        uint32_t tick = HAL_GetTick();
        GPIO_PinState state = HAL_GPIO_ReadPin(BUTTON_GPIO_BANK, BUTTON_GPIO_PIN);

        // Check for a press or release, and debounce
        if (state == 1 && !pressed) {
            if (press_debounce == 0) {
                press_debounce = tick;
            } else if (tick - press_debounce > DEBOUNCE_PERIOD) {
                press_debounce = 0;
                pressed = true;
            }
        }

        if (state == 0 && pressed) {
            if (release_debounce == 0) {
                release_debounce = tick;
            } else if (tick - release_debounce > DEBOUNCE_PERIOD) {
                release_debounce = 0;
                show_count = !show_count;
                pressed = false;
            }
        }

        // Periodically update the display and flash the USER LED
        if (tick - last_tick > DEFAULT_TASK_PAUSE) {
            last_tick = tick;
            HAL_GPIO_TogglePin(LED_GPIO_BANK, LED_GPIO_PIN);
            if (use_i2c) {
                if (show_count) {
                    HT16K33_show_value(counter, false);
                } else {
                    HT16K33_show_value((uint16_t)(temp * 100), true);
                    HT16K33_set_alpha('c', 3, false);
                }

                HT16K33_draw();
                counter++;
                if (counter > 9999) counter = 0;
            }
        }
    }
}


/**
  * @brief  Function implementing the Debug Task thread.
  * @param  argument: Not used
  * @retval None
  */
void start_iot_task(void *argument) {
    uint32_t count = 0;

    // Get the Device ID and build number
    uint8_t buffer[35] = { 0 };
    mvGetDeviceId(buffer, 34);
    printf("Dev ID: ");
    printf((char *)buffer);
    printf("\n");
    printf("Build: %i\n", BUILD_NUM);

    got_sensor = MCP9808_init();
    if (got_sensor) temp = MCP9808_read_temp();

    // Run the thread's main loop
    while (true) {
        if (got_sensor) {
            temp = MCP9808_read_temp();
            if (count % 10 == 0) printf("[DEBUG] Temperature: %.02f\n", temp);
        }

        // Pause per cycle
        osDelay(DEBUG_TASK_PAUSE);
        count++;
    }
}


void http_open_channel(void) {
    // Clear any existing interrupt
    NVIC_ClearPendingIRQ(TIM8_BRK_IRQn);

    // Clear the notification store
    memset((void *)http_notification_center, 0xFF, sizeof(http_notification_center));

    // Configure a multi-se notification center for network-centric notifications
    static struct MvNotificationSetup http_notification_setup = {
        .irq = TIM8_BRK_IRQn,
        .buffer = (struct MvNotification *)http_notification_center,
        .buffer_size = sizeof(http_notification_center)
    };

    // Ask Microvisor to establish the notification center
    // and confirm that it has accepted the request
    uint32_t status = mvSetupNotifications(&http_notification_setup, &http_handles.notification);
    assert(status == MV_STATUS_OKAY);

    // Action interrupt
    NVIC_EnableIRQ(TIM8_BRK_IRQn);
    printf("[DEBUG] Channel notification centre handle: %lu\n", (uint32_t)http_handles.notification);

    // Set up the HTTP channel's multi-use send and receive buffers
    static volatile uint8_t http_rx_buffer[1536] __attribute__((aligned(512)));
    static volatile uint8_t http_tx_buffer[512] __attribute__((aligned(512)));
    const char endpoint[] = "";
    http_handles.network = get_net_handle();
    assert(http_handles.network != 0);
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
    status = mvOpenChannel(&channel_config, &http_handles.channel);
    if (status == MV_STATUS_OKAY) {
        printf("[DEBUG] HTTP channel open. Handle: %lu\n", (uint32_t)http_handles.channel);
        assert(http_handles.channel != 0);
    } else {
        printf("[ERROR] HTTP channel closed. Status: %lu\n", status);
    }
}


void http_close_channel(void) {
    // If we have a valid channel handle -- ie. it is non-zero --
    // then ask Microvisor to close it and confirm acceptance of
    // the closure request.
    if (http_handles.channel != 0) {
        uint32_t status = mvCloseChannel(&http_handles.channel);
        http_handles.channel = 0;
        printf("[DEBUG] HTTP channel closed\n");
        assert(status == MV_STATUS_OKAY);
    }

    // Confirm the channel handle has been invalidated by Microvisor
    assert(http_handles.channel == 0);
}


bool http_send_request() {
    // Check for a valid channel handle
    if (http_handles.channel != 0) {
        printf("[DEBUG] Sending HTTP request\n");

        // Set up the request -- this is basic
        const char verb[] = "GET";
        const char uri[] = "https://jsonplaceholder.typicode.com/todos/1";
        const char body[] = "";
        struct MvHttpHeader hdrs[] = {};
        struct MvHttpRequest request_config = {
            .method = (uint8_t *)verb,
            .method_len = strlen(verb),
            .url = (uint8_t *)uri,
            .url_len = strlen(uri),
            .num_headers = 0,
            .headers = hdrs,
            .body = (uint8_t *)body,
            .body_len = strlen(body),
            .timeout_ms = 10000
        };

        // Issue the request -- and check its status
        uint32_t status = mvSendHttpRequest(http_handles.channel, &request_config);

        if (status != MV_STATUS_OKAY) {
            printf("[ERROR] Could not issue request. Status: %lu\n", status);
            return false;
        }

        printf("[DEBUG] Request sent to Twilio\n");
        return true;
    }

    http_open_channel();
    return http_send_request();
}


void TIM8_BRK_IRQHandler(void) {

    enum MvEventType event_kind = http_notification_center->event_type;
    printf("[DEBUG] Channel notification center IRQ called for event: %u\n", event_kind);

    if (event_kind == MV_EVENTTYPE_CHANNELDATAREADABLE) {
        static struct MvHttpResponseData resp_data;
        uint32_t status = mvReadHttpResponseData(http_handles.channel, &resp_data);
        if (status == MV_STATUS_OKAY) {
            printf("[DEBUG] HTTP response result: %u\n", resp_data.result);
            printf("[DEBUG] HTTP response header count: %lu\n", resp_data.num_headers);
            printf("[DEBUG] HTTP response body length: %lu\n", resp_data.body_length);
            printf("[DEBUG] HTTP status code: %lu\n", resp_data.status_code);

            if (resp_data.result == MV_HTTPRESULT_OK) {
                if (resp_data.status_code == 200) {
                    uint8_t buffer[resp_data.body_length];
                    status = mvReadHttpResponseBody(http_handles.channel, 0, buffer, resp_data.body_length);

                    if (status == MV_STATUS_OKAY) {
                        printf((char *)buffer);
                        printf("\n");
                    } else {
                        printf("[ERROR] HTTP response body read status %lu\n", status);
                    }
                }
            }
        } else {
            printf("[ERROR] Response data read failed. Status: %lu\n", status);
        }

        request_recv = true;
    }
}
