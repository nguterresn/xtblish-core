#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <sys/_stdint.h>

int wasm_boot_app(bool skip_runtime_init);
int wasm_replace_app(uint8_t* src, uint32_t len);
