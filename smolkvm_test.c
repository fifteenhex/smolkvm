#include "smolkvm.h"

int main(int argc, char **argv, char **envp)
{
	struct smolkvm_vm vm = { 0 };
	int ret;

	printf("Built %s @ %s\n", __DATE__, __TIME__);

	ret = smolkvm_create_vm(&vm);
	if (ret) {
		printf("smolkvm_create_vm() failed: %d\n", ret);
		return 1;
	}

	smolkvm_dump_memory_map(&vm);

	ret = smolkvm_load_elf_file(&vm, "ipl/ipl.elf");

#ifdef SMOLKVM_WANT_GDB_STUB
	__smolkvm_gdb_stub_start(&vm);
	__smolkvm_gdb_stub_accept(&vm);
#endif

	smolkvm_default_loop(&vm);

	printf("Destroying vm\n");
	smolkvm_destroy_vm(&vm);

	return 0;
}
