all: ipl

build:
	meson setup build

.PHONY: ipl

ipl: | build
	meson compile -C build

.PHONY: clean

clean:
	rm -rf build
