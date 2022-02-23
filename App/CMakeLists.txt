cmake_minimum_required(VERSION 3.14)

# Application data
set(APP_TITLE "Microvisor IoT Device Demo")
set(VERSION_NUMBER "1.0.2")
set(BUILD_NUMBER "1")

message("Building ${APP_TITLE} ${VERSION_NUMBER} build ${BUILD_NUMBER}")

# Make app data available to the compiler
add_compile_definitions(BUILD_NUM=${BUILD_NUMBER})
add_compile_definitions(APP_NAME="${APP_TITLE}")
add_compile_definitions(APP_VERSION="${VERSION_NUMBER}")

# Read in the target URL from env var MVIOT_URL
if(DEFINED ENV{MVIOT_URL})
    add_compile_definitions(API_URL="$ENV{MVIOT_URL}")
else()
    add_compile_definitions(API_URL="UNDEFINED")
endif()

MESSAGE(STATUS "URL set to: $ENV{MVIOT_URL}")

# Compile app source code file(s)
add_executable(mv-iot-device-demo
    main.c
    ht16k33.c
    http.c
    i2c.c
    logging.c
    mcp9808.c
    stm32u5xx_hal_timebase_tim_template.c
)

# Link libraries
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
)

add_custom_target(extras ALL DEPENDS EXTRAS)
