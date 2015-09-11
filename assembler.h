#ifndef ASSEMBLER_H
#define ASSEMBLER_H
#include "common.h"

int reg_mem_codegen(char *cmd, char **args);
int reg_num_codegen(char *cmd, char **args);
int reg_nums_mask_codegen(char *cmd, char **args);
int no_operands_codegen(char *cmd, char **args);
int label_addr_codegen(char *cmd, char **args);
int irmovl_codegen(char *cmd, char **args);
int long_codegen(char *cmd, char **args);
int pos_codegen(char *cmd, char **args);
int align_codegen(char *cmd, char **args);
int gen_bytecode(char *filename);
int gen_yo_file(char *filename);
int get_instr_size(char *instr_name, uint16 addr);

#endif
