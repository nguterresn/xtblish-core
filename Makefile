# Nothing for now.

config:
	cmake -S . -B build

build: config
	cmake --build build

run: build
	./build/app

zephyr:
	west build -b esp32_devkitc_wroom/esp32/procpu

menuconfig:
	west build -b esp32_devkitc_wroom/esp32/procpu -t menuconfig

ram_report:
	west build -b esp32_devkitc_wroom/esp32/procpu -t ram_report > ram_report.txt

rom_report:
	west build -b esp32_devkitc_wroom/esp32/procpu -t rom_report > rom_report.txt

flash: zephyr
	/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py --chip auto --baud 230400 --before default_reset --after hard_reset write_flash -u --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 /Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/build/zephyr/zephyr.bin 0x250000 /Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/application/assem/build/release.wasm

dump:
		/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py --chip auto --baud 230400 read_flash 0x250000 0x6000 wasm_dump.bin
		xxd wasm_dump.bin | less

clean:
	rm -rf build

.PHONY: build config clean run zephyr menuconfig rom_report ram_report

