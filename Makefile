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

VARIANTS := _libc _debug_libc _gdb_libc _debug_gdb_libc

SIMPLE_TARGETS := $(addprefix smolkvm_test_simple,$(VARIANTS))
APIC_TARGETS   := $(addprefix smolkvm_test_apic,$(VARIANTS))
ALL_TARGETS    := $(SIMPLE_TARGETS) $(APIC_TARGETS)

# The generated header describes the APIC machine (that is what the IPL boots),
# so it is dumped by the plain APIC build.
HDR_GEN := smolkvm_test_apic_libc

all: $(ALL_TARGETS) $(IPL)

$(SIMPLE_TARGETS): MACHINETYPE := -DSMOLKVM_WANT_SIMPLE
$(APIC_TARGETS):   MACHINETYPE := -DSMOLKVM_WANT_APIC

$(filter %_debug_libc %_debug_gdb_libc,$(ALL_TARGETS)): EXTRA_FLAGS += $(DEBUG_FLAGS)
$(filter %_gdb_libc,$(ALL_TARGETS)):                    EXTRA_FLAGS += $(GDB_FLAGS)

$(ALL_TARGETS): $(SRC) $(HDR)
	$(CC) $(COPTS) $(MACHINETYPE) $(EXTRA_FLAGS) -o $@ $(SRC)

# `-h` only reads compile-time constants, so this needs no /dev/kvm and works
# on a bare build host (CI, cross builds).
ipl/include/machine.h: $(HDR_GEN)
	./$(HDR_GEN) -h $@

.PHONY: $(IPL)
$(IPL): ipl/include/machine.h
	$(MAKE) -C ipl/

ifdef NOLIBCDIR
all: smolkvm_test smolkvm_test_gdb

smolkvm_test: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -DSMOLKVM_WANT_APIC -nostdlib -include $(NOLIBCDIR)/nolibc.h $(COPTS) -static -o $@ $< -lgcc

# -DSMOLKVM_WANT_GDB_STUB_DEBUG
smolkvm_test_gdb: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -DSMOLKVM_WANT_APIC -DSMOLKVM_WANT_GDB_STUB -nostdlib -include $(NOLIBCDIR)/nolibc.h $(COPTS) -static -o $@ $< -lgcc
else
$(warning Please pass NOLIBCDIR with the path to your copy of nolibc (tools/include/nolibc/ in the linux source) for static targets)
endif

.PHONY: clean
clean:
	rm -f $(ALL_TARGETS) smolkvm_test smolkvm_test_gdb
	rm -f ipl/include/machine.h
	$(MAKE) -C ipl/ clean 2>/dev/null || true
