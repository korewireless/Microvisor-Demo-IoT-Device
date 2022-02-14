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

/**
 *  Theses variables may be changed by interrupt handler code,
 *  so we mark them as `volatile` to ensure compiler optimization
 *  doesn't render them immutable at runtime
 */
volatile bool use_i2c = false;
volatile bool got_sensor = false;
volatile bool show_count = false;
volatile bool request_recv = false;
volatile uint16_t counter = 0;
volatile double temp = 0.0;

/**
 * These variables are defined in `http.c`
 */
extern struct {
    MvNotificationHandle notification;
    MvNetworkHandle      network;
    MvChannelHandle      channel;
} http_handles;


/**
 * @brief The application entry point.
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
  * @brief Get the MV clock value.
  *
  * @returns The clock value.
  */
uint32_t SECURE_SystemCoreClockUpdate() {
    uint32_t clock = 0;
    mvGetHClk(&clock);
    return clock;
}


/**
  * @brief System clock configuration.
  */
void system_clock_config(void) {
    SystemCoreClockUpdate();
    HAL_InitTick(TICK_INT_PRIORITY);
}


/**
  * @brief Initialize the MCU GPIO
  *
  * Used to flash the Nucleo's USER LED, which is on GPIO Pin PA5.
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
  * @brief Function implementing the display task thread.
  *
  * @param argument: Not used.
  */
void start_led_task(void *argument) {
    uint32_t last_tick = 0;

    // Button state values
    uint32_t press_debounce = 0;
    uint32_t release_debounce = 0;
    bool pressed = false;

    // Set up the display if it's available
    if (use_i2c) HT16K33_init(&i2c);

    // The task's main loop
    while (true) {
        // Get the ms timer value and read the button
        uint32_t tick = HAL_GetTick();
        GPIO_PinState state = 0; //HAL_GPIO_ReadPin(BUTTON_GPIO_BANK, BUTTON_GPIO_PIN);

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

        // End of cycle delay
        osDelay(10);
    }
}


/**
  * @brief Function implementing the Debug Task thread.
  *
  * @param argument: Not used.
  */
void start_iot_task(void *argument) {
    // Get the Device ID and build number
    log_device_info();

    // Set up channel notifications
    http_channel_center_setup();

    // Prep the sensor (if present)
    got_sensor = MCP9808_init();
    if (got_sensor) temp = MCP9808_read_temp();

    // Time trackers
    uint32_t read_tick = 0;
    uint32_t kill_time = 0;
    bool close_channel = false;

    // Run the thread's main loop
    while (true) {
        uint32_t tick = HAL_GetTick();

        if (got_sensor) {
            temp = MCP9808_read_temp();

            if (tick - read_tick > SENSOR_READ_PERIOD) {
                // Read the sensor every 30s
                read_tick = tick;
                printf("\n[DEBUG] Temperature: %.02f\n", temp);

                // No channel open? Try and send the temperature
                if (http_handles.channel == 0) {
                    http_open_channel();
                    bool result = http_send_request(temp);
                    if (!result) close_channel = true;
                    kill_time = tick;
                } else {
                    printf("[ERROR] Channel handle not zero\n");
                }
            }
        }

        // Process a request's response if indicated by the ISR
        if (request_recv) {
            http_process_response();
        }

        // Use 'kill_time' to force-close an open HTTP channel
        // if it's been left open too long
        if (kill_time > 0 && tick - kill_time > CHANNEL_KILL_PERIOD) {
            close_channel = true;
        }

        // Close the channel if asked to do so or
        // a request yielded a response
        if (close_channel || request_recv) {
            close_channel = false;
            request_recv = false;
            kill_time = 0;
            http_close_channel();
        }

        // End of cycle delay
        osDelay(10);
    }
}





/**
 * @brief Show basic device info.
 */
void log_device_info(void) {
    uint8_t buffer[35] = { 0 };
    mvGetDeviceId(buffer, 34);
    printf("Device: %s\n   App: %s\n Build: %i\n", buffer, APP_NAME, BUILD_NUM);
}


/**
 * @brief Log an error message
 *
 * @param msg:   A pointer to a message string containing one long unsigned int marker.
 * @param value: A 32-bit unsigned int to be interpolated into `msg`.
 */
void log_error(const char* msg, uint32_t value) {
    char print_str[80] = {0};
    strcpy(print_str, "[ERROR] ");

    if (strlen(msg) > 61) {
        char trunc_str[61] = {0};
        strncpy(trunc_str, msg, 61);
        sprintf(&print_str[8], trunc_str, value);
    } else {
        sprintf(&print_str[8], msg, value);
    }

    // Output the final string
    printf(print_str);
    printf("\n");
}


/**
 * @brief Interpolate a 32-bit unsigned int into a string
 *
 * @param out_str: A pointer to storage for the formatted string. 80 chars max.
 * @param in_str:  A pointer to a message string containing one long unsigned int marker.
 * @param value:   A 32-bit unsigned int to be interpolated into `msg`.
 */
void format_string(char* out_str, const char* in_str, uint32_t value) {
    char* base = malloc(80 * sizeof(char));
    sprintf(base, in_str, value);
    strcpy(out_str, in_str);
    free(base);
}
