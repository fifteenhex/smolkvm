# SMoLKVM

```

┌┐┌┬┐┌┐┬ ┬┌\   /┬┐
└┐││││││ ├┴┐\ /│││
└┘┴ ┴└┘┴─┴ └┘× ┴ ┴
 ~dgp

```

## What

This is a single header implementation of a very crappy "virtual machine".

## Features

- One vcpu, its basically impossible to do any better without threads.
- Dumb console emulation to a unix domain socket so you can connect minicom.
- Direct entry into long mode.
- Two machine flavours: SIMPLE (polled MMIO irqchip + timer, for bare-metal
  guests) and APIC (KVM's in-kernel APIC/IOAPIC/PIC/PIT + a legacy COM1
  UART, boots stock Linux).
- meh grade gdb stub.
- Optional virtio-gpu, scanned out to VNC viewers by smolrfb. See below.
- lots of bugs
- Compiles to a completely self standing static binary with nolibc: pass
  both `NOLIBCDIR` (your nolibc, e.g. `tools/include/nolibc` in the linux
  source) and `NOLIBCEXTDIR` (a checkout of nolibc-extensions, which adds
  the sockets/fcntl/signal bits nolibc doesn't carry yet):

  ```
  make NOLIBCDIR=/path/to/nolibc NOLIBCEXTDIR=/path/to/nolibc-extensions
  ```

  builds `smolkvm_test` and `smolkvm_test_gdb` as static nolibc binaries.

## Booting Linux

The APIC machine (`SMOLKVM_WANT_APIC`, what the Makefile builds) boots an
uncompressed ELF kernel (`vmlinux`, NOT a bzImage) via the 64-bit boot
protocol. The IPL provides the smallest ACPI table set that keeps Linux
happy (MADT so it finds the LAPIC/IOAPIC, a legacy -- deliberately not
hardware-reduced -- FADT, an empty DSDT) and a classic PC e820. The guest
console is a bare-bones 16550 at the usual 0x3f8/irq4, so `console=ttyS0`
just works; output is mirrored to stdout and to the unix socket.

```
make
./smolkvm_test_apic_libc -k /path/to/vmlinux -r initramfs.cpio.gz -c "console=ttyS0 rdinit=/bin/sh"
```

`-m` sets the RAM size in MB (default 64). The kernel needs
`CONFIG_SERIAL_8250_CONSOLE=y`; there is no PCI, no disk and no network,
so an initramfs is the only way to get userspace.

## A display

`SMOLKVM_WANT_VIRTIO_GPU` adds a virtio-gpu, and
[smolrfb](https://github.com/fifteenhex/smolrfb) serves its scanout to VNC
viewers. It is off unless asked for, and needs the APIC machine (the driver
wants a real interrupt) plus a smolrfb checkout to include:

```
make SMOLRFBDIR=/path/to/smolrfb
./smolkvm_test_apic_gpu_libc -k vmlinux -r initramfs.cpio.gz \
	-c "console=tty0 console=ttyS0" -g 1280x800 -p 5900
```

and point a VNC viewer at `127.0.0.1:5900`. `console=tty0` is what puts the
kernel log on the display; without it you get the boot logo on black and no way
to tell a working display from a stuck one. Note also that Linux stops reading
its own parameters at a standalone `--`, so anything after that goes to init
rather than the kernel -- smolkvm puts its own `virtio_mmio.device=` in front of
one for you.

`-g` sets the display size and `-p` the port; both default to 1024x768 on 5900.
The server binds loopback only -- there is no authentication in smolrfb, so
reach it from elsewhere over an ssh tunnel.

The transport is virtio-mmio rather than virtio-pci, because there is no PCI
here. That means nothing enumerates the device: the guest is told where it is
on the kernel command line, and `smolkvm_test` appends the right
`virtio_mmio.device=...` fragment for you (it is printed at startup if you are
building your own command line). The guest kernel needs
`CONFIG_VIRTIO_MMIO_CMDLINE_DEVICES=y` -- without it nothing parses the
fragment and no device appears -- along with `CONFIG_DRM_VIRTIO_GPU`,
`CONFIG_DRM_FBDEV_EMULATION` and the VT layer if you want a console on it.
`test/kernel-gpu.config` is that fragment.

Everything the test pipeline downloads, unpacks, clones or builds lands in
`build/`, and nothing checked in lives under there -- so `rm -rf build` (or
`make -f Makefile.test distclean`) is always safe. The config fragments it
needs are in `test/`.

```
make -f Makefile.test smoke-gpu
```

boots the whole lot and passes once the console has moved onto the display.

The device is 2D only: no virgl, no blob resources, no EDID.

### Doom

The smoke test's rootfs can also carry
[doomgeneric](https://github.com/fifteenhex/doomgeneric) drawing through
[smol2d](https://github.com/fifteenhex/smol2d)'s DRM backend, with id's
shareware wad bundled alongside it:

```
make -f Makefile.test doom            # just the tarball doomgeneric builds
make -f Makefile.test doom-initramfs  # the smolutils rootfs, with doom in it
make -f Makefile.test smoke-gpu       # boot that, with the display
```

None of it lives here: doomgeneric packages itself, and its `Makefile.rootfs`
fetches smol2d and the wad and hands back a tarball. All this end does is clone
it and ask, passing the nolibc out of the kernel it already unpacked.

It lands at `/usr/bin/doom` with the wad at `/usr/share/doom/doom1.wad`.
doomgeneric's own iwad search is compiled out, so say where it is:

```
doom -iwad /usr/share/doom/doom1.wad
```
