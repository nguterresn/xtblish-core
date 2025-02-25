#include "bindings.h"
#include <zephyr/sys/printk.h>

void bd_console_log(wasm_exec_env_t exec_env, const uint16_t* str)
{
	printk("[Native]: ");
	while (*str) {
		printk("%c", (char)*str);
		str++;
	}

	printk("\n");
}

void bd_abort(wasm_exec_env_t exec_env, const uint16_t* msg, const uint16_t* filename,
              uint32_t file_line, uint32_t col_line)
{
	printk("[Native]: WASM module abort line %d col %d", file_line, col_line);
}
