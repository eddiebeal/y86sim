#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "common.h"
#include "debugger.h"
#include "parser.h"

#define EAX 0
#define ECX 1
#define EDX 2
#define EBX 3
#define ESP 4
#define EBP 5
#define ESI 6
#define EDI 7

// USED BY DO_ARITHMETIC //
#define ARITH_ADD 0
#define ARITH_SUB 1
#define ARITH_XOR 2
#define ARITH_AND 3
#define ARITH_MULT 4
#define ARITH_DIV 5
#define ARITH_MOD 6

// USED BY PUSH AND POP //
#define STACK_RAW_VAL 0
#define STACK_REG_VAL 1

typedef struct flags {
	uint32 OF, SF, ZF;
} Flags;

typedef struct instruction {
	char name[16]; // name of the instruction
	uint8 opcode;
	int size;
	int num_args; /* number of arguments the command takes */
	int (*cmd_callback)(); /* returns 1 if the callback succeeds
							  and 0 if it does not */
} Instruction;

typedef struct _StackFrame {
	char func_name[MAX_LABEL_NAME];
	uint16 addr;
	uint32 esp;
	struct _StackFrame *next;
} StackFrame;

extern StackFrame *stack_frames;
  
extern uint8 memory[4096];
extern int mem_len; // used by assembler
extern uint32 registers[8];
extern Flags flgs;
extern Instruction instrs[];
extern int num_instrs;

void sim_init_registers();
void sim_init_flags();
uint16 sim_get_pc();
void sim_set_pc(uint16 new_PC);
int irmovl_callback();
int rmmovl_callback();
int mrmovl_callback();
int rrmovl_callback();
int rdint_callback();
int rdch_callback();
int wrint_callback();
int wrch_callback();
int nop_callback();
int halt_callback();
uint32 do_arithmetic(uint32 src, uint32 dest, int op, int *err);
int addl_callback();
int subl_callback();
int multl_callback();
int divl_callback();
int xorl_callback();
int andl_callback();
int modl_callback();
int jmp_callback();
int je_callback();
int jle_callback();
int jg_callback();
int jl_callback();
int jne_callback();
int jge_callback();
uint32 pushl(uint8 src_reg, uint32 val, int op, int *err);
uint32 popl(uint32 dest_reg, int op, int *err);
int pushl_callback();
int popl_callback();
int call_callback();
int ret_callback();
void write_condition_list(FILE *out, ConditionList *list);
ConditionList *read_condition_list(FILE *in);
void sim_exec_bytecode();
SourceLine *read_source_lines(FILE *in);

#endif
