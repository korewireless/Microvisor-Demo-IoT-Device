cmake_minimum_required(VERSION 3.14)

# Set application data
set(APP "Microvisor IoT Device Demo")
set(VERSION_NUMBER "3.1.1")
set(BUILD_NUMBER "3")

message("Building ${APP} ${VERSION_NUMBER} build ${BUILD_NUMBER}")

# FROM 3.0.2
# Pass in version data
configure_file(app_version.in app_version.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Read in the target URL from env var MVIOT_URL
if(DEFINED ENV{MVIOT_URL})
    add_compile_definitions(API_URL="$ENV{MVIOT_URL}")
else()
    add_compile_definitions(API_URL="UNDEFINED")
endif()

message("URL set to: $ENV{MVIOT_URL}")

# Compile app source code file(s)
add_executable(${PROJECT_NAME}
    ht16k33-seg.c
    http.c
    i2c.c
    lis3dh.c
    logging.c
    main.c
    mcp9808.c
    network.c
    uart_logging.c
    stm32u5xx_hal_timebase_tim_template.c
)

# Link libraries
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC
    ST_Code
    Microvisor-HAL-STM32U5
    FreeRTOS)

# Optional informational and additional format generation
# NOTE From 2.0.3, this generates an alternative .bin file
#      than was previously the case
add_custom_command(OUTPUT EXTRA_FILES DEPENDS ${PROJECT_NAME}
    COMMAND mv "${PROJECT_NAME}" "${PROJECT_NAME}.elf"
    COMMAND ${CMAKE_SIZE} --format=berkeley "${PROJECT_NAME}.elf"
    COMMAND ${CMAKE_OBJDUMP} -h -S "${PROJECT_NAME}.elf" > "${PROJECT_NAME}.list"
    COMMAND ${CMAKE_OBJCOPY} --output-target ihex "${PROJECT_NAME}.elf" "${PROJECT_NAME}.hex"
    COMMAND ${CMAKE_OBJCOPY} --input-target ihex --output-target binary --gap-fill 0xFF "${PROJECT_NAME}.hex" "${PROJECT_NAME}.bin"
)

# Prepare the additional files
add_custom_target(extras ALL DEPENDS EXTRA_FILES)
