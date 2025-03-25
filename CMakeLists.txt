cmake_minimum_required (VERSION 3.20)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project (wasm-zephyr-ota)
# enable_language(ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Reset default linker flags
set (CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set (CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-U,_get_ext_lib_export_apis")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

# Install all the npm packages if they don't exist.
if (NOT EXISTS ${CMAKE_SOURCE_DIR}/application/assem/node_modules)
  execute_process(COMMAND npm update WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/application/assem COMMAND_ERROR_IS_FATAL ANY)
endif()

include(wasm.cmake) # Include WASM related CMake variables.

set(WAMR_ROOT_DIR ${CMAKE_SOURCE_DIR}/libs/wasm-micro-runtime)
include (${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

# Compile Assembly Script and generate debug.wasm #

file(GLOB TS_FILES ${CMAKE_SOURCE_DIR}/application/assem/assembly/*.ts)
set(WASM_OUTPUT ${CMAKE_SOURCE_DIR}/application/assem/build/debug.wasm)
set(WASM_AS_INDEX ${CMAKE_SOURCE_DIR}/application/assem/assembly/index.ts)
set(BINARY_OUTPUT ${CMAKE_SOURCE_DIR}/application/assem/build/signed-debug.bin)

add_custom_target(rel-wasm ALL DEPENDS ${WASM_OUTPUT})
add_custom_command(
  OUTPUT ${WASM_OUTPUT} ${BINARY_OUTPUT}
  DEPENDS ${TS_FILES}
  COMMENT "Running xtblish-client: compile, sign and deploy WASM application"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/application/assem
  COMMAND npx xtblish -s assembly/index.ts -u 124 -c xtblish.json --dev
)

FILE(GLOB_RECURSE app_sources application/*.c)
target_sources(app PRIVATE ${WAMR_RUNTIME_LIB_SOURCE} ${app_sources})
target_include_directories(app PRIVATE
  ${CMAKE_SOURCE_DIR}/application
  ${CMAKE_SOURCE_DIR}/application/wasm
  ${CMAKE_SOURCE_DIR}/application/network
)

# Print app and wasm binary size
add_custom_command(
  POST_BUILD
  TARGET app
  COMMAND ls -l ${CMAKE_BINARY_DIR}/app
  COMMAND ls -l ${WASM_OUTPUT}
  COMMAND ls -l ${BINARY_OUTPUT}
)
