cmake_minimum_required(VERSION 3.14)

set(BUILD_NUMBER "1")
add_definitions(-DBUILD_NUM=${BUILD_NUMBER})

add_executable(mv-iot-device
    main.c
    logging.c
    i2c.c
    ht16k33.c
    mcp9808.c
    stm32u5xx_hal_timebase_tim_template.c
)

target_link_libraries(mv-iot-device LINK_PUBLIC
    ST_Code
    twilio-microvisor-hal-stm32u5
    FreeRTOS)

# Optional informational and additional format generation
# including Bundle output
add_custom_command(OUTPUT EXTRAS
    DEPENDS mv-iot-device
    COMMAND cp "mv-iot-device" "mv-iot-device.elf"
    COMMAND ${CMAKE_SIZE} --format=berkeley "mv-iot-device"
    COMMAND ${CMAKE_OBJDUMP} -h -S "mv-iot-device.elf" > "mv-iot-device.list"
    COMMAND ${CMAKE_OBJCOPY} -O binary "mv-iot-device.elf" "mv-iot-device.bin"
    COMMAND python3 ../../twilio-microvisor-tools/bundler-py/bundler.py mv-iot-device.elf mv-iot-device.zip
    COMMAND rm -f ../../null.d
    COMMAND echo "LATEST BUILD: ${BUILD_NUMBER}"

)

add_custom_target(extras ALL
    ${CMAKE_COMMAND} -E echo "Outputting additional artefacts"
    DEPENDS EXTRAS
)
