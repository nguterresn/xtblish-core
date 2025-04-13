# Nothing for now.

IMAGE_0_VERSION := 0.0.1
IMAGE_0_SLOT_SIZE := 1048576
IMAGE_0_OFFSET := 0x10000

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
	python3 ../bootloader/mcuboot/scripts/imgtool.py create --version $(IMAGE_0_VERSION) --align 4 --header-size 32 --slot-size $(IMAGE_0_SLOT_SIZE) build/zephyr/zephyr.bin dist/unsigned-image0.bin
	python3 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py -p /dev/cu.wchusbserial58FC0505471 --baud 921600 --before default_reset --after hard_reset write_flash -u --flash_mode dio --flash_freq 40m --flash_size detect $(IMAGE_0_OFFSET) dist/unsigned-image0.bin

erase-image0:
	/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py -p /dev/cu.wchusbserial58FC0505471 --baud 921600 erase_region $(IMAGE_0_OFFSET) 0x100000

dump-app0:
		/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py --chip auto --baud 921600 read_flash 0x210000 0x6000 dump_app0.bin
		# xxd dump_app0.bin | less

dump-wasm0:
		/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py --chip auto --baud 921600 read_flash 0x210224 0x6000 dump_wasm0.wasm

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

.PHONY: build config clean run zephyr flash menuconfig rom_report ram_report

