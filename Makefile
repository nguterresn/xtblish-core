# Nothing for now.

# Tools

ESPTOOL := ../modules/hal/espressif/tools/esptool_py/esptool.py
IMGTOOL := ../bootloader/mcuboot/scripts/imgtool.py

# Build

BOARD := esp32s3_devkitc/esp32s3/procpu

IMAGE_0_VERSION := 0.0.1
IMAGE_SLOT_SIZE := 0x100000

IMAGE_0_OFFSET := 0x20000
IMAGE_1_OFFSET := 0x120000

APP_0_OFFSET := 0x220000
APP_1_OFFSET := 0x420000

APP_DUMP_SIZE := 0x6000

UNSIGNED_IMAGE := dist/unsigned_image.bin
SIGNED_IMAGE := dist/signed_image.bin

SIGN_KEYS_PATH := dist/sign_image_keys.pem
REPORT := ram_report

PORT := /dev/tty.wchusbserial58FC0505471

list:
	ls /dev/tty.*

config:
	cmake -S . -B build

build: config
	cmake --build build

run: build
	./build/app

zephyr:
	west build -b $(BOARD)

menuconfig:
	west build -b $(BOARD) -t menuconfig

report:
	west build -b $(BOARD) -t $(REPORT) > $(REPORT).txt

image-keys:
	# python3 $(IMGTOOL) keygen -k dist/image_keys.pem -t ed25519

plain: zephyr
	python3 $(IMGTOOL) create \
	 	--version $(IMAGE_0_VERSION) --pad \
	 	--align 4 --header-size 32 \
		--slot-size $(IMAGE_SLOT_SIZE) \
		build/zephyr/zephyr.bin $(UNSIGNED_IMAGE)

sign: zephyr
	python3 $(IMGTOOL) sign \
		--key $(SIGN_KEYS_PATH) \
		--version $(IMAGE_0_VERSION) \
		--pad --pad-sig \
		--align 4 --header-size 32 \
		--slot-size $(IMAGE_SLOT_SIZE) \
		build/zephyr/zephyr.bin $(SIGNED_IMAGE)

dump-sign:
	python3 $(IMGTOOL) dumpinfo $(SIGNED_IMAGE)

flash-plain: plain
	python3 $(ESPTOOL) \
		-p $(PORT) \
		--baud 921600 --before default_reset \
		--after hard_reset write_flash \
		-u --flash_mode dio --flash_freq 40m \
		--flash_size detect $(IMAGE_0_OFFSET) $(UNSIGNED_IMAGE)

flash-sign: sign
	python3 $(ESPTOOL) \
		-p $(PORT) \
		--baud 921600 --before default_reset \
		--after hard_reset write_flash \
		-u --flash_mode dio --flash_freq 40m \
		--flash_size detect $(IMAGE_0_OFFSET) $(SIGNED_IMAGE)

erase-image0:
	python3 $(ESPTOOL) -p $(PORT) --baud 921600 erase_region $(IMAGE_0_OFFSET) 0x100000

erase-all:
	python3 $(ESPTOOL) -p $(PORT) --baud 921600 erase_all

dump-app0:
	python3 $(ESPTOOL) -p $(PORT) --baud 921600 \
		read_flash $(APP_0_OFFSET) $(APP_DUMP_SIZE) dump_app0.bin

dump-app1:
	python3 $(ESPTOOL) -p $(PORT) --baud 921600 \
	 read_flash $(APP_1_OFFSET) $(APP_DUMP_SIZE) dump_app1.bin

monitor:
	west espressif monitor -p $(PORT)

ocd:
	/Applications/openocd-esp32/bin/openocd -c 'set ESP_RTOS none;' -f /Applications/openocd-esp32/share/openocd/scripts/board/esp32s3-builtin.cfg

clean:
	rm -rf build

.PHONY: build config clean run zephyr sign flash-plain flash-sign image-keys plain menuconfig rom_report ram_report

