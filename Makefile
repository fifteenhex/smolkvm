all: smolkvm_test_gdb_libc \
	smolkvm_test_debug_libc \
	smolkvm_test_debug_gdb_libc \
	smolkvm_test_libc \
	$(IPL)

#	-Wall \
#	-flto \

COPTS= -ggdb \
	-std=c99 \
	-Os

IPL=ipl/build/ipl

ipl/include/machine.h: smolkvm_test_libc
	./smolkvm_test_libc -h $@

.PHONY: $(IPL)
$(IPL): ipl/include/machine.h
	$(MAKE) -C ipl/

ifdef NOLIBCDIR
all: smolkvm_test smolkvm_test_gdb

smolkvm_test: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -nostdlib -include $(NOLIBCDIR)/nolibc.h $(COPTS) -static -o $@ $< -lgcc

# -DSMOLKVM_WANT_GDB_STUB_DEBUG
smolkvm_test_gdb: smolkvm_test.c smolkvm.h $(IPL)
	$(CC) -DSMOLKVM_WANT_GDB_STUB -nostdlib -include $(NOLIBCDIR)/nolibc.h $(COPTS) -static -o $@ $< -lgcc
else
$(warning Please pass NOLIBCDIR with the path to your copy of nolibc (tools/include/nolibc/ in the linux source) for static targets)
endif


smolkvm_test_libc: smolkvm_test.c smolkvm.h
	$(CC) $(COPTS) -o $@ $<

smolkvm_test_debug_libc: smolkvm_test.c smolkvm.h
	$(CC) $(COPTS) -DSMOLKVM_DEBUG -o $@ $<

smolkvm_test_gdb_libc: smolkvm_test.c smolkvm.h
	$(CC) -DSMOLKVM_WANT_GDB_STUB -DSMOLKVM_WANT_GDB_STUB_DEBUG $(COPTS) -o $@ $<


smolkvm_test_debug_gdb_libc: smolkvm_test.c smolkvm.h
	$(CC) -DSMOLKVM_DEBUG -DSMOLKVM_WANT_GDB_STUB -DSMOLKVM_WANT_GDB_STUB_DEBUG $(COPTS) -o $@ $<

.PHONY: clean
clean:
	rm -f smolkvm_test smolkvm_test_gdb
