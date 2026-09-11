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
./smolkvm_test_libc -k /path/to/vmlinux -r initramfs.cpio.gz -c "console=ttyS0 rdinit=/bin/sh"
```

`-m` sets the RAM size in MB (default 64). The kernel needs
`CONFIG_SERIAL_8250_CONSOLE=y`; there is no PCI, no disk and no network,
so an initramfs is the only way to get userspace.
