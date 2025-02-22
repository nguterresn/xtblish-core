#include "wasm_export.h"
#include "wasm_runtime.h"
#include "wasm_runtime_common.h"
#include <stdint.h>
#include <stdio.h>

static unsigned char* read_wasm_file(const char* filename, size_t* size);

/**
 * @brief Port console.log to the native side. Provide a way to log from WASM modules.
 * @note String argument is UTF-16 based.
 *
 * @param exec_env
 * @param str UTF-16 string
 */
static void console_log(wasm_exec_env_t exec_env, const uint16_t* str);

static void _abort(wasm_exec_env_t exec_env, const uint16_t* msg, const uint16_t* filename,
                   uint32_t file_line, uint32_t col_line);

static NativeSymbol native_symbols[] = { {
	                                         "console.log", // the name of WASM function name
	                                         console_log,   // the native function pointer
	                                         "($)"          // the function prototype signature
	                                     },
	                                     {
	                                         "abort", // the name of WASM function name
	                                         _abort,  // the native function pointer
	                                         "($$ii)" // the function prototype signature
	                                     } };

int main(void)
{
	char   error_buf[256] = { 0 };
	uint32 stack_size = 4096, heap_size = 0;
	size_t wasm_file_size = 0;

	bool result = wasm_runtime_init();
	if (!result) {
		printf("Failed to initialize the runtime.\n");
	}
	wasm_runtime_set_log_level(WASM_LOG_LEVEL_VERBOSE);

	/* read WASM file into a memory buffer */
	uint8_t* wasm_ptr = read_wasm_file("application/assem/build/release.wasm", &wasm_file_size);
	if (!wasm_ptr) {
		printf("Failed to read the WASM file\n");
		return 1;
	}

	if (!wasm_runtime_register_natives("env",
	                                   native_symbols,
	                                   sizeof(native_symbols) / sizeof(NativeSymbol))) {
		printf("Failed to export native symbols!");
		return 1;
	}

	// /* parse the WASM file from buffer and create a WASM module */
	wasm_module_t module = wasm_runtime_load(wasm_ptr,
	                                         wasm_file_size,
	                                         error_buf,
	                                         sizeof(error_buf));
	if (module == NULL) {
		printf("Failed to load! Error: %s\n", error_buf);
		return 1;
	}

	/* create an instance of the WASM module (WASM linear memory is ready) */
	wasm_module_inst_t module_inst = wasm_runtime_instantiate(module,
	                                                          0, // May be ignored.
	                                                          heap_size,
	                                                          error_buf,
	                                                          sizeof(error_buf));
	if (module_inst == NULL) {
		printf("Failed to instantiate! Error: %s\n", error_buf);
		return 1;
	}

	// The stack size is the amount of memory necessary to store
	// the WASM binary operations, like i32.add 1 or i32.const 1
	wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);
	if (exec_env == NULL) {
		printf("Failed to create the environment!\n");
		return 1;
	}

	wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, "hello_world");
	if (func == NULL) {
		printf("Failed to look up for 'hello_world'\n");
		return 1;
	}

	wasm_runtime_call_wasm(exec_env, func, 0, NULL);
}

void console_log(wasm_exec_env_t exec_env, const uint16_t* str)
{
	printf("[Native]: ");
	while (*str) {
		printf("%c", (char)*str);
		str++;
	}

	printf("\n");
}

void _abort(wasm_exec_env_t exec_env, const uint16_t* msg, const uint16_t* filename,
            uint32_t file_line, uint32_t col_line)
{
	printf("[Native]: WASM module abort line %d col %d", file_line, col_line);
}

unsigned char* read_wasm_file(const char* filename, size_t* size)
{
	FILE* file = fopen(filename, "rb");
	if (!file) {
		fprintf(stderr, "Failed to open file '%s': %s\n", filename, strerror(errno));
		return NULL;
	}

	// Get file size
	fseek(file, 0, SEEK_END);
	*size = ftell(file);
	fseek(file, 0, SEEK_SET);

	// Allocate memory for the entire file
	unsigned char* buffer = (unsigned char*)malloc(*size);
	if (!buffer) {
		fprintf(stderr, "Failed to allocate memory for file content\n");
		fclose(file);
		return NULL;
	}

	// Read the file into the buffer
	size_t read_size = fread(buffer, 1, *size, file);
	fclose(file);

	if (read_size != *size) {
		fprintf(stderr, "Failed to read entire file\n");
		free(buffer);
		return NULL;
	}

	return buffer;
}
