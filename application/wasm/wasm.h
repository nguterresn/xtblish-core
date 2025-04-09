#pragma once
#include <stdint.h>
#include <stdbool.h>

int wasm_init();
int wasm_replace(void);
int wasm_write_app1(uint8_t* src, uint32_t len, uint32_t sector_offset,
                    bool force);
int wasm_verify_and_copy(uint32_t app1_write_len);
