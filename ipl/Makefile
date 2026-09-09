all: ipl

build:
	meson setup build --cross-file cross.x86_64

.PHONY: ipl

ipl: | build
	meson compile -C build

.PHONY: clean

clean:
	rm -rf build
