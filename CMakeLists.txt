cmake_minimum_required (VERSION 3.20)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project (wasm-zephyr-ota)
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

set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/wasm-micro-runtime)
include (${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)

set(WASM_OUTPUT ${CMAKE_SOURCE_DIR}/application/build/release.wasm)

# The target 'generate_wasm' requires WASM_OUTPUT, so if it doesn't exist,
# the next command will generate it.
add_custom_target(generate_wasm ALL DEPENDS ${WASM_OUTPUT})
add_custom_command(
  OUTPUT ${WASM_OUTPUT}
  COMMENT "Compiling AssemblyScript to WASM."
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/application/assem
  COMMAND npx asc assembly/index.ts --initialMemory 1 --maximumMemory 1 --lowMemoryLimit
)

add_executable(app ${WAMR_RUNTIME_LIB_SOURCE} application/main.c)

# Print app and wasm binary size
add_custom_command(
  POST_BUILD
  TARGET app
  COMMAND ls -l ${CMAKE_BINARY_DIR}/app
  COMMAND ls -l ${CMAKE_SOURCE_DIR}/application/assem/build/release.wasm
)
