#include "wasm/wasm.h"
#include "network/http.h"

int main(void)
{
	http_init();
	wasm_boot_app();
	return 0;
}
