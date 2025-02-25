#pragma once

#include "wasm_export.h"

/**
 * @brief Port console.log to the native side. Provide a way to log from WASM
 * @note String argument is UTF-16 based.
 *
 * @param exec_env
 * @param str UTF-16 string
 */
void bd_console_log(wasm_exec_env_t exec_env, const uint16_t* str);
void bd_abort(wasm_exec_env_t exec_env, const uint16_t* msg, const uint16_t* filename,
              uint32_t file_line, uint32_t col_line);
