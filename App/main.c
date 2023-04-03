/**
 *
 * Microvisor IoT Device Demo
 * Version 3.0.1
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */
#include "main.h"


/*
 * STATIC PROTOTYPES
 */
static void system_clock_config(void);
static void GPIO_init(void);
static void led_task(void *argument);
static void iot_task(void *argument);
static void process_http_response(void);
static void log_device_info(void);


/*
 * GLOBALS
 */
// This is the FreeRTOS thread task that flashed the USER LED
// and operates the display
static osThreadId_t task_led;
static const osThreadAttr_t led_task_attributes = {
    .name = "LEDTask",
    .stack_size = 2560,
    .priority = (osPriority_t)osPriorityNormal
};

// This is the FreeRTOS thread task that reads the sensor
// and displays the temperature on the LED
static osThreadId_t task_iot;
static const osThreadAttr_t iot_task_attributes = {
    .name = "IOTTask",
    .stack_size = 5120,
    .priority = (osPriority_t)osPriorityNormal
};

// I2C-related values
I2C_HandleTypeDef i2c;


/**
 *  Theses variables may be changed by interrupt handler code,
 *  so we mark them as `volatile` to ensure compiler optimization
 *  doesn't render them immutable at runtime
 */
volatile bool use_i2c = false;
volatile bool received_request = false;
volatile bool channel_was_closed = false;

static volatile double temp = 0.0;
static volatile bool is_connected = false;
static volatile bool interrupt_triggered = false;
static volatile bool got_sensor_temp = false;
static volatile bool got_sensor_accl = false;

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
    
    // Get the Device ID and build number
    log_device_info();
    
    // Start the network
    net_open_network();

    // Initialize the peripherals
    GPIO_init();
    I2C_init();
    
    // FROM 1.1.0
    // Signal app start on LED
    // (`use_i2c` set by `I2C_init()`)
    if (use_i2c) {
        // Set up the display if it's available
        HT16K33_init();

        // Write 'boot' to the LED
        HT16K33_set_glyph(0x7C, 0, false);
        HT16K33_set_glyph(0x5C, 1, false);
        HT16K33_set_glyph(0x5C, 2, false);
        HT16K33_set_glyph(0x78, 3, false);
        HT16K33_draw();

        // Wait 1.5 seconds
        uint32_t tick = HAL_GetTick();
        while (HAL_GetTick() - tick < 1500) {
            // NOP
        }
        
        // FROM 2.1.6 -- prep the sensors here
        got_sensor_temp = MCP9808_init();
        got_sensor_accl = LIS3DH_init();
    }
    
    // Prep the MCP9808 temperature sensor (if present)
    if (got_sensor_temp) temp = MCP9808_read_temp();

    // Prep the LIS3DH accelerometer (if present)
    if (got_sensor_accl) {
        // This is not necessary for power-cycled devices,
        // but handy when updating code w/o power-cycling
        // the sensor
        LIS3DH_reset();

        // Configure the LIS3DH
        LIS3DH_set_mode(LIS3DH_MODE_NORMAL);
        LIS3DH_set_data_rate(100);
        LIS3DH_configure_click_irq(true, LIS3DH_SINGLE_CLICK, 1.1, 5, 10, 50);
        LIS3DH_configure_irq_latching(true);
    }

    // Init scheduler
    osKernelInitialize();

    // Create the thread(s)
    task_iot = osThreadNew(iot_task, NULL, &iot_task_attributes);
    task_led = osThreadNew(led_task, NULL, &led_task_attributes);

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
static void system_clock_config(void) {
    
    SystemCoreClockUpdate();
    HAL_InitTick(TICK_INT_PRIORITY);
}


/**
 * @brief Initialize the MCU GPIO
 *
 * Used to flash the Nucleo's USER LED, which is on GPIO Pin PA5.
 * and as an interrupt source (GPIO Pin PF3) connected to the
 * LIS3DH motion sensor.
 */
static void GPIO_init(void) {
    
    // Enable GPIO port clock
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    // Configure GPIO pin output Level
    HAL_GPIO_WritePin(LED_GPIO_BANK, LED_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LIS3DH_INT_GPIO_BANK, LIS3DH_INT_GPIO_PIN, GPIO_PIN_RESET);

    // Configure GPIO pin for the on-board LED
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    GPIO_InitStruct.Pin   = LED_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LED_GPIO_BANK, &GPIO_InitStruct);

    // Configure GPIO pin for the LIS3DH interrupt
    GPIO_InitTypeDef GPIO_InitStruct2 = { 0 };
    GPIO_InitStruct2.Pin   = LIS3DH_INT_GPIO_PIN;
    GPIO_InitStruct2.Mode  = GPIO_MODE_IT_RISING;
    GPIO_InitStruct2.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(LIS3DH_INT_GPIO_BANK, &GPIO_InitStruct2);

    // Set up the NVIC to process interrupts
    HAL_NVIC_SetPriority(LIS3DH_INT_IRQ, 3, 0);
    HAL_NVIC_EnableIRQ(LIS3DH_INT_IRQ);
}


/**
 * @brief Function implementing the display task thread.
 *
 * @param argument: Not used.
 */
static void led_task(void *argument) {
    
    uint32_t last_tick = 0;

    // The task's main loop
    while (true) {
        // Get the ms timer value and read the button
        uint32_t tick = HAL_GetTick();

        // Periodically update the display and flash the USER LED
        if (tick - last_tick > DEFAULT_TASK_PAUSE_MS) {
            // Flip the USER LED
            HAL_GPIO_TogglePin(LED_GPIO_BANK, LED_GPIO_PIN);
            last_tick = tick;
        }
        
        // Check connection state
        is_connected = false;
        if (http_handles.network != 0) {
            enum MvNetworkStatus net_state = MV_NETWORKSTATUS_DELIBERATELYOFFLINE;
            enum MvStatus status = mvGetNetworkStatus(http_handles.network, &net_state);

            if (status == MV_STATUS_OKAY) {
                is_connected = (net_state == MV_NETWORKSTATUS_CONNECTED);
            }
        } else {
            http_handles.network = net_get_handle();
        }

        // Display the temperature
        if (use_i2c) {
            HT16K33_show_value((uint16_t)(temp * 100), true);
            HT16K33_set_alpha('c', 3, !is_connected);
            HT16K33_draw();
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
static void iot_task(void *argument) {
    
    // Time trackers
    uint32_t read_tick = 0;
    uint32_t kill_time = 0;
    bool do_close_channel = false;
    enum MvStatus result = MV_STATUS_OKAY;
    
    // Set up channel notifications
    http_notification_center_setup();

    // Run the thread's main loop
    while (true) {
        uint32_t tick = HAL_GetTick();

        if (got_sensor_temp) {
            // Get the temperature
            temp = MCP9808_read_temp();
            
            if (tick - read_tick > SENSOR_READ_PERIOD_MS) {
                // Read the sensor every x seconds
                read_tick = tick;
                server_log("Temperature: %.02f°C", temp);
                
                // No channel open? Try and send the temperature
                if (http_handles.channel == 0 && http_open_channel()) {
                    result = http_send_request(temp);
                    if (result > 0) do_close_channel = true;
                    kill_time = tick;
                } else {
                    server_error("Channel handle not zero");
                    do_close_channel = true;
                }
            }
            
            // Process a request's response if indicated by the ISR
            if (received_request) process_http_response();
            
            // FROM 2.1.6
            // Was the channel closed unexpectedly?
            // `channel_was_closed` set in IRS
            if (channel_was_closed) do_close_channel = true;
            
            // Use 'kill_time' to force-close an open HTTP channel
            // if it's been left open too long
            if (kill_time > 0 && tick - kill_time > CHANNEL_KILL_PERIOD_MS) {
                do_close_channel = true;
                server_error("HTTP request timed out");
            }
            
            // Close the channel if asked to do so or
            // a request yielded a response
            if (do_close_channel || received_request) {
                do_close_channel = false;
                received_request = false;
                kill_time = 0;
                http_close_channel();
            }
        }
        
        // Was an interrupt triggered? If so, log the fact
        if (interrupt_triggered) {
            interrupt_triggered = false;
            server_log("Interrupt signal on GPIO PF3");

            if (use_i2c) {
                InterruptTable table;
                LIS3DH_get_interrupt_table(&table);
                if (table.single_click) {
                    server_log("Device tapped once");
                    http_send_warning();
                }

                AccelResult accel;
                LIS3DH_get_accel(&accel);
                server_log("Acceleration X:%0.2fG, Y:%0.2fG, Z:%0.2fG", accel.x, accel.y, accel.z);
            }
        }

        // End of cycle delay
        osDelay(10);
    }
}


/**
 * @brief Process HTTP response data.
 */
static void process_http_response(void) {
    
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
 * @brief Show basic device info.
 */
static void log_device_info(void) {
    
    uint8_t buffer[35] = { 0 };
    mvGetDeviceId(buffer, 34);
    server_log("Device: %s", buffer);
    server_log("   App: %s %s", APP_NAME, APP_VERSION);
    server_log(" Build: %i", BUILD_NUM);
}


/**
 * @brief Report an error (numeric code) on the 4-digit LED.
 */
void report_and_assert(uint16_t err_code) {
    
    // Display the error code on the LED
    HT16K33_clear_buffer();
    HT16K33_show_value(err_code, false);
    HT16K33_set_glyph(0x79, 0, false);
    HT16K33_set_glyph(0x00, 1, false);
    HT16K33_draw();
    
    server_log("ASSERTING (%i)", err_code);
    
    // Halt everything
    vTaskSuspendAll();
    assert(false);
}


/**
 * @brief IRQ handler as specified in HAL doc.
 *
 * @param GPIO_Pin The pin the triggered the IRQ
 */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    
    interrupt_triggered = true;
}


/**
 * @brief IRQ handler as specified in HAL doc.
 *
 * @param GPIO_Pin The pin the triggered the IRQ
 */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    
    interrupt_triggered = true;
}


/**
 * @brief Interrupt handler as specified in HAL doc.
 */
void EXTI3_IRQHandler(void) {
    
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}
