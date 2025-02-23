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

clean:
	rm -rf build

.PHONY: build config clean run zephyr menuconfig rom_report ram_report

