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

# Compile Assembly Script and generate release.wasm #

file(GLOB TS_FILES ${CMAKE_SOURCE_DIR}/application/assem/assembly/*.ts)
set(WASM_OUTPUT ${CMAKE_SOURCE_DIR}/application/assem/build/release.wasm)
set(WASM_AS_INDEX ${CMAKE_SOURCE_DIR}/application/assem/assembly/index.ts)

add_custom_target(rel-wasm ALL DEPENDS ${WASM_OUTPUT})
add_custom_command(
  OUTPUT ${WASM_OUTPUT}
  DEPENDS ${TS_FILES}
  COMMENT "Compiling AssemblyScript to WASM."
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/application/assem
  COMMAND npx asc ${WASM_AS_INDEX}
)

# Take the release.wasm and use it to create another binary file #

set(BINARY_OUTPUT ${CMAKE_BINARY_DIR}/wasm.bin)

add_custom_target(wasm-bin ALL DEPENDS ${BINARY_OUTPUT})
add_custom_command(
  OUTPUT ${BINARY_OUTPUT}
  DEPENDS ${WASM_OUTPUT}
  COMMENT "Creating the binary file for WASM"
  COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/create_binary.py -i ${WASM_OUTPUT} -o ${BINARY_OUTPUT}
)

target_sources(app PRIVATE ${WAMR_RUNTIME_LIB_SOURCE}
  application/main.c
  application/wasm/wasm.c
  application/wasm/bindings.c
)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/application/wasm)

# Print app and wasm binary size
add_custom_command(
  POST_BUILD
  TARGET app
  COMMAND ls -l ${CMAKE_BINARY_DIR}/app
  COMMAND ls -l ${CMAKE_SOURCE_DIR}/application/assem/build/release.wasm
)
