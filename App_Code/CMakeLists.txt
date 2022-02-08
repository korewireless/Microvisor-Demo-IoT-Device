cmake_minimum_required(VERSION 3.14)

# Add your Beeceptor URL and path (/api/v1/data) here
add_compile_definitions(API_URL="https://twilio-test.free.beeceptor.com/api/v1/data")
add_compile_definitions(APP_NAME="Microvisor IoT Device Demo")

set(BUILD_NUMBER "1")
add_compile_definitions(BUILD_NUM=${BUILD_NUMBER})

add_executable(mv-iot-device-demo
    main.c
    logging.c
    i2c.c
    ht16k33.c
    mcp9808.c
    stm32u5xx_hal_timebase_tim_template.c
)

target_link_libraries(mv-iot-device-demo LINK_PUBLIC
    ST_Code
    twilio-microvisor-hal-stm32u5
    FreeRTOS)

# Optional informational and additional format generation
# including Bundle output
add_custom_command(OUTPUT EXTRAS
    DEPENDS mv-iot-device-demo
    COMMAND cp "mv-iot-device-demo" "mv-iot-device-demo.elf"
    COMMAND ${CMAKE_SIZE} --format=berkeley "mv-iot-device-demo"
    COMMAND ${CMAKE_OBJDUMP} -h -S "mv-iot-device-demo.elf" > "mv-iot-device-demo.list"
    COMMAND ${CMAKE_OBJCOPY} -O binary "mv-iot-device-demo.elf" "mv-iot-device-demo.bin"
    COMMAND python3 ../../twilio-microvisor-tools/bundler-py/bundler.py mv-iot-device-demo.elf mv-iot-device-demo.zip
    COMMAND rm -f ../../null.d
    COMMAND echo "LATEST BUILD: ${BUILD_NUMBER}"
)

add_custom_target(extras ALL
    ${CMAKE_COMMAND} -E echo "Outputting additional artefacts"
    DEPENDS EXTRAS
)
