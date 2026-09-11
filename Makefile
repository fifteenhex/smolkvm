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
ifndef NOLIBCEXTDIR
$(warning Please also pass NOLIBCEXTDIR with the path to your nolibc-extensions checkout for static targets)
else
all: smolkvm_test smolkvm_test_gdb

# Force-include nolibc, then the extensions umbrella on top -- the order lets
# the extensions defer to anything a given nolibc already carries. The
# extensions supply what smolkvm needs beyond stock nolibc: unix + tcp
# sockets, fcntl(), and the sigset helpers.
NOLIBC_INC = -include $(NOLIBCDIR)/nolibc.h \
	     -include $(NOLIBCEXTDIR)/include/nolibc-extensions.h

smolkvm_test: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -DSMOLKVM_WANT_APIC -nostdlib $(NOLIBC_INC) $(COPTS) -static -o $@ $< -lgcc

# -DSMOLKVM_WANT_GDB_STUB_DEBUG
smolkvm_test_gdb: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -DSMOLKVM_WANT_APIC -DSMOLKVM_WANT_GDB_STUB -nostdlib $(NOLIBC_INC) $(COPTS) -static -o $@ $< -lgcc
endif
else
$(warning Please pass NOLIBCDIR with the path to your copy of nolibc (tools/include/nolibc/ in the linux source) for static targets)
endif

.PHONY: clean
clean:
	rm -f $(ALL_TARGETS) smolkvm_test smolkvm_test_gdb
	rm -f ipl/include/machine.h
	$(MAKE) -C ipl/ clean 2>/dev/null || true
