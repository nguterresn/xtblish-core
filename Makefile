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

clean:
	rm -rf build

.PHONY: build config clean run zephyr menuconfig

