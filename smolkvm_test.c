#include "smolkvm.h"

#define MAILBOX_CMD_GETPARAMS (SMOLKVM_MAILBOX_CMD_MINUSER + 0)

struct cmd_buffer_getparams {
	uint64_t gpa;
	uint64_t size;
};

static uint64_t get_params_fn(struct smolkvm_vm *vm, uint64_t command, void *buffer, void *priv) {

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	struct cmd_buffer_getparams getparams = { 0 };
	struct smolkvm_vm vm = { 0 };
	int ret;

	printf("Built %s @ %s\n", __DATE__, __TIME__);

	ret = smolkvm_create_vm(&vm);
	if (ret) {
		printf("smolkvm_create_vm() failed: %d\n", ret);
		return 1;
	}

	ret = smolkvm_mailbox_register(&vm, MAILBOX_CMD_GETPARAMS, &getparams,
				     sizeof(getparams), get_params_fn, NULL);
	if (ret)
		return 1;

	smolkvm_dump_memory_map(&vm);

	ret = smolkvm_load_elf_file(&vm, "ipl/build/ipl");

#ifdef SMOLKVM_WANT_GDB_STUB
	__smolkvm_gdb_stub_start(&vm);
	ret = __smolkvm_gdb_stub_accept(&vm);
	if (ret) {
		printf("GDB stub accept failed: %d\n", ret);
		return 1;
	}
#endif

	smolkvm_default_loop(&vm);

	printf("Destroying vm\n");
	smolkvm_destroy_vm(&vm);

	return 0;
}
