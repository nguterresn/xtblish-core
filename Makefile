# Nothing for now.

config:
	cmake -S . -B build

build: config
	cmake --build build

run: build
	./build/app

zephyr:
	west build -b esp32s3_devkitc/esp32s3/procpu

menuconfig:
	west build -b esp32s3_devkitc/esp32s3/procpu -t menuconfig

ram_report:
	west build -b esp32s3_devkitc/esp32s3/procpu -t ram_report > ram_report.txt

rom_report:
	west build -b esp32s3_devkitc/esp32s3/procpu -t rom_report > rom_report.txt

flash: zephyr
	/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py -p /dev/cu.wchusbserial58FC0505471 --baud 921600 --before default_reset --after hard_reset write_flash -u --flash_mode dio --flash_freq 40m --flash_size detect 0x0 /Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/build/zephyr/zephyr.bin

dump-app0:
		/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py --chip auto --baud 921600 read_flash 0x210000 0x6000 dump_app0.bin
		# xxd dump_app0.bin | less

dump-app1:
		/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py --chip auto --baud 921600 read_flash 0x410000 0x6000 dump_app1.bin
		# xxd dump_app1.bin | less

monitor:
	west espressif monitor -p /dev/cu.wchusbserial58FC0505471
	# west espressif monitor -p /dev/cu.usbmodem1101

ocd:
	/Applications/openocd-esp32/bin/openocd -c 'set ESP_RTOS none;' -f /Applications/openocd-esp32/share/openocd/scripts/board/esp32s3-builtin.cfg

clean:
	rm -rf build

.PHONY: build config clean run zephyr menuconfig rom_report ram_report

