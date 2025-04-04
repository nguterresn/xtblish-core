#pragma once
#include <stdint.h>
#include <stdbool.h>

#define WASM_FILE_MAX_SIZE 6144 // In words!

int wasm_init();
int wasm_replace(void);
int wasm_write_app1(uint8_t* src, uint32_t len, bool last);
int wasm_verify_and_copy(uint32_t app1_write_len);
