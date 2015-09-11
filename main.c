#include <stdio.h>
#include "console.h"
#include "assembler.h"
#include "simulator.h"
#include "common.h"

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Usage: %s <y86 source file>\n", argv[0]);
		return 0;
	}

	init_dbg_print();
	init_console();
	
	switch (gen_bytecode(argv[1])) {
	case SUCC:
		sim_init_registers();
		sim_init_flags();
		sim_exec_bytecode();
		break;
	case INVALID_FILE:
		destroy_console(); // need to destory console so we can use printf again
		printf("Error opening %s for reading\n", argv[1]);
		break;
	case PARSE_ERROR:
		destroy_console();
		printf("Error parsing %s\n", argv[1]);
		break;
	}
	
	destroy_console();
	destroy_dbg_print();
	return 0;
}
