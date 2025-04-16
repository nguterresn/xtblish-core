# Nothing for now.

IMAGE_0_VERSION := 0.0.1
IMAGE_SLOT_SIZE := 1048576

IMAGE_0_OFFSET := 0x17000
IMAGE_1_OFFSET := 0x117000

APP_0_OFFSET := 0x217000
APP_1_OFFSET := 0x417000

UNSIGNED_IMAGE := dist/unsigned_image.bin
SIGNED_IMAGE := dist/signed_image.bin

SIGN_KEYS_PATH := dist/image_keys.pem

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

image-keys:
	# python3 ../bootloader/mcuboot/scripts/imgtool.py keygen -k dist/image_keys.pem -t ed25519

plain: zephyr
	python3 ../bootloader/mcuboot/scripts/imgtool.py create \
	 	--version $(IMAGE_0_VERSION) \
	 	--align 4 \
		--header-size 32 \
		--slot-size $(IMAGE_SLOT_SIZE) \
		build/zephyr/zephyr.bin $(UNSIGNED_IMAGE)

sign: zephyr
	python3 ../bootloader/mcuboot/scripts/imgtool.py sign \
		--key $(SIGN_KEYS_PATH)
		--version $(IMAGE_0_VERSION) \
		--align 4 \
		--header-size 32 \
		--slot-size $(IMAGE_SLOT_SIZE) \
		build/zephyr/zephyr.bin $(SIGNED_IMAGE)

flash-plain: plain
	python3 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py \
		-p /dev/cu.wchusbserial58FC0505471 \
		--baud 921600 --before default_reset --after hard_reset write_flash \
		-u --flash_mode dio --flash_freq 40m --flash_size detect $(IMAGE_0_OFFSET) $(UNSIGNED_IMAGE)

flash-sign: sign
	python3 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py \
		-p /dev/cu.wchusbserial58FC0505471 \
		--baud 921600 --before default_reset --after hard_reset write_flash \
		-u --flash_mode dio --flash_freq 40m --flash_size detect $(IMAGE_0_OFFSET) $(SIGNED_IMAGE)

erase-image0:
	python3 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py \
		-p /dev/cu.wchusbserial58FC0505471 \
		--baud 921600 \
		erase_region $(IMAGE_0_OFFSET) 0x100000

erase-all:
	python3 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py \
		-p /dev/cu.wchusbserial58FC0505471 \
		--baud 921600 \
		erase_flash

dump-app0:
		/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py --chip auto --baud 921600 read_flash $(APP_0_OFFSET) 0x6000 dump_app0.bin
		# xxd dump_app0.bin | less

dump-app1:
		/Users/nunonogueira/Projectos/zephyr-projects/wasm-zephyr-ota/.venv/bin/python3.13 /Users/nunonogueira/Projectos/zephyr-projects/modules/hal/espressif/tools/esptool_py/esptool.py --chip auto --baud 921600 read_flash $(APP_1_OFFSET) 0x6000 dump_app1.bin

monitor:
	west espressif monitor -p /dev/cu.wchusbserial58FC0505471

ocd:
	/Applications/openocd-esp32/bin/openocd -c 'set ESP_RTOS none;' -f /Applications/openocd-esp32/share/openocd/scripts/board/esp32s3-builtin.cfg

clean:
	rm -rf build

.PHONY: build config clean run zephyr sign flash-plain flash-sign image-keys plain menuconfig rom_report ram_report

