#pragma once
#include <stdint.h>
#include <stdbool.h>

#define WASM_FILE_MAX_SIZE 6144 // In words!

int wasm_init();
int wasm_load_app(uint8_t* src, uint32_t len);
