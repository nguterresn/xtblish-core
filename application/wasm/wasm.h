#pragma once
#include <stdint.h>
#include <stdbool.h>

#define WASM_FILE_MAX_SIZE 6144 // In words!

int wasm_boot_app(bool skip_runtime_init);
int wasm_replace_app(uint8_t* src, uint32_t len);
