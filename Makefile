# Must be defined before it appears in a prerequisite list: make expands
# prerequisites when it reads the rule, so a later definition is just empty.
IPL=ipl/build/ipl

COPTS= -ggdb \
	-std=c99 \
	-Os

#	-Wall \
#	-flto \

SRC := smolkvm_test.c
HDR := smolkvm.h

DEBUG_FLAGS := -DSMOLKVM_DEBUG
GDB_FLAGS   := -DSMOLKVM_WANT_GDB_STUB -DSMOLKVM_WANT_GDB_STUB_DEBUG
# The input device takes its events from the display's viewers, so the two are
# built together. Drop SMOLKVM_WANT_VIRTIO_INPUT for the display on its own.
GPU_FLAGS   := -DSMOLKVM_WANT_VIRTIO_GPU -DSMOLKVM_WANT_VIRTIO_INPUT \
	       -I$(SMOLRFBDIR)

VARIANTS := _libc _debug_libc _gdb_libc _debug_gdb_libc

SIMPLE_TARGETS := $(addprefix smolkvm_test_simple,$(VARIANTS))
APIC_TARGETS   := $(addprefix smolkvm_test_apic,$(VARIANTS))
ALL_TARGETS    := $(SIMPLE_TARGETS) $(APIC_TARGETS)

# The virtio-gpu needs smolrfb, which lives in its own repo, so its targets only
# exist once you say where that is. Only the APIC machine can have one: the
# driver wants a real interrupt.
ifdef SMOLRFBDIR
GPU_TARGETS    := $(addprefix smolkvm_test_apic_gpu,$(VARIANTS))
ALL_TARGETS    += $(GPU_TARGETS)
endif

GPU_SELFTEST := smolkvm_gpu_selftest

# The generated header describes the APIC machine (that is what the IPL boots),
# so it is dumped by the plain APIC build.
HDR_GEN := smolkvm_test_apic_libc

all: $(ALL_TARGETS) $(IPL)

$(SIMPLE_TARGETS): MACHINETYPE := -DSMOLKVM_WANT_SIMPLE
$(APIC_TARGETS):   MACHINETYPE := -DSMOLKVM_WANT_APIC
$(GPU_TARGETS):    MACHINETYPE := -DSMOLKVM_WANT_APIC

$(filter %_debug_libc %_debug_gdb_libc,$(ALL_TARGETS)): EXTRA_FLAGS += $(DEBUG_FLAGS)
$(filter %_gdb_libc,$(ALL_TARGETS)):                    EXTRA_FLAGS += $(GDB_FLAGS)
$(GPU_TARGETS):                                         EXTRA_FLAGS += $(GPU_FLAGS)

$(ALL_TARGETS): $(SRC) $(HDR)
	$(CC) $(COPTS) $(MACHINETYPE) $(EXTRA_FLAGS) -o $@ $(SRC)

# The virtio-gpu's own test. It drives the device model through plain function
# calls rather than a vCPU, so unlike the smoke test it needs no /dev/kvm. The
# sanitizers are on because most of what it feeds in is guest supplied.
ifdef SMOLRFBDIR
all: $(GPU_SELFTEST)

$(GPU_SELFTEST): $(GPU_SELFTEST).c $(HDR)
	$(CC) $(COPTS) -O1 -fsanitize=address,undefined -DSMOLKVM_WANT_APIC \
		$(GPU_FLAGS) -I. -o $@ $<

.PHONY: check-gpu
check-gpu: $(GPU_SELFTEST)
	./$(GPU_SELFTEST)
endif

# `-h` only reads compile-time constants, so this needs no /dev/kvm and works
# on a bare build host (CI, cross builds).
ipl/include/machine.h: $(HDR_GEN)
	./$(HDR_GEN) -h $@

.PHONY: $(IPL)
$(IPL): ipl/include/machine.h
	$(MAKE) -C ipl/

ifdef NOLIBCDIR
ifndef NOLIBCEXTDIR
$(warning Please also pass NOLIBCEXTDIR with the path to your nolibc-extensions checkout for static targets)
else
all: smolkvm_test smolkvm_test_gdb

# Force-include nolibc, then the extensions umbrella on top so they can defer
# to whatever a given nolibc already carries. The extensions supply what
# smolkvm needs beyond stock nolibc: unix + tcp sockets, fcntl(), sigsets.
NOLIBC_INC = -include $(NOLIBCDIR)/nolibc.h \
	     -include $(NOLIBCEXTDIR)/include/nolibc-extensions.h

smolkvm_test: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -DSMOLKVM_WANT_APIC -nostdlib $(NOLIBC_INC) $(COPTS) -static -o $@ $< -lgcc

# -DSMOLKVM_WANT_GDB_STUB_DEBUG
smolkvm_test_gdb: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -DSMOLKVM_WANT_APIC -DSMOLKVM_WANT_GDB_STUB -nostdlib $(NOLIBC_INC) $(COPTS) -static -o $@ $< -lgcc

# smolrfb builds with nolibc too, so the machine with a display is still a
# single static binary.
ifdef SMOLRFBDIR
all: smolkvm_test_gpu

smolkvm_test_gpu: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -DSMOLKVM_WANT_APIC $(GPU_FLAGS) -nostdlib $(NOLIBC_INC) $(COPTS) -static -o $@ $< -lgcc
endif
endif
else
$(warning Please pass NOLIBCDIR with the path to your copy of nolibc (tools/include/nolibc/ in the linux source) for static targets)
endif

.PHONY: clean
clean:
	rm -f $(ALL_TARGETS) smolkvm_test smolkvm_test_gdb smolkvm_test_gpu $(GPU_SELFTEST)
	rm -f ipl/include/machine.h
	$(MAKE) -C ipl/ clean 2>/dev/null || true
