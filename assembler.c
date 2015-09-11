// assembler.c - Contains code to generate the raw bytecode interpreted by the simulator
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include "simulator.h"
#include "common.h"
#include "assembler.h"
#include "debugger.h"
#include "parser.h"

// Writes a byte to memory
void write_uint8(uint8 val) {
	memory[mem_len++] = val;
}

// Writes a 4 byte integer to memory
void write_uint32(uint32 val) {
	*((uint32*)&memory[mem_len]) = val;
	mem_len += 4;
}

/*
  Codegen function for instructions whose operands are structured as follows:
    BYTE: 4 higher bits register number
          4 lower bits register number
    UINT32: Offset
   
  Supports rmmovl, mrmovl
*/
int reg_mem_codegen(char *cmd, char **args) {
	char *reg = args[0]; // source for rmmovl (dest for mrmovl) e.g. %ecx -- TODO: add check to verify valid register (use reg_num)
	char *mem_addr = args[1]; // dest for rmmovl (source for mrmovl), e.g. 16(%ebx) or MyLabel
  
	if (strcmp(cmd, "rmmovl") == 0) {
		write_uint8(0x40);
		reg = args[0];
		mem_addr = args[1];
	} else {
		write_uint8(0x50);
		reg = args[1];
		mem_addr = args[0];
	}
  
	if (strstr(mem_addr, "%") != NULL) {
		// mem addr is supplied in format <offset>(%reg)
		uint32 offset = 0;
		char *open_paren = strchr(mem_addr, '(');
		char *close_paren = strchr(mem_addr, ')');
		char *mem_addr_reg = strchr(mem_addr, '%') + 1;
		
		if (open_paren == NULL || close_paren == NULL) {
			DBG_PRINT("Error, parens missing.\n");
			return 0;
		}
		
		*open_paren = '\0';
		*close_paren = '\0';
		
		if (valid_stol_str(mem_addr))
			offset = stol(mem_addr);
		
		DBG_PRINT("Supplied as reg: offset=%d, mem_addr_reg=%s, reg=%s\n", offset, mem_addr_reg, reg);
		
		write_uint8((reg_name_to_num(reg + 1) << 4) |
					reg_name_to_num(mem_addr_reg));
		write_uint32(offset);
	} else {
		// mem_addr is supplied as a label
		char *label_name;
    
		if (strcmp(cmd, "rmmovl") == 0)
			label_name = args[1];
		else
			label_name = args[0];
		
		DBG_PRINT("Supplied as label - %s\n", label_name);
		
		Label *label = find_label(label_name);
		
		if (label == NULL) {
			DBG_PRINT("Invalid label\n");
			return 0;
		}

		write_uint8((reg_name_to_num(reg + 1) << 4) | 8);
		write_uint32(label->addr);
	}
	
	return 1;
}

/*
  Codegen function for instructions that have a single
    operand, one byte long, storing a register number
    
  Supports rdint, rdch, wrint, wrch, pushl, popl
*/
int reg_num_codegen(char *cmd, char **args) {
	if (strcmp(cmd, "rdint") == 0)
		write_uint8(0xf2);
	else if (strcmp(cmd, "rdch") == 0)
		write_uint8(0xf0);
	else if (strcmp(cmd, "wrint") == 0)
		write_uint8(0xf3);
	else if (strcmp(cmd, "wrch") == 0)
		write_uint8(0xf1);
	else if (strcmp(cmd, "pushl") == 0)
		write_uint8(0xa0);
	else if (strcmp(cmd, "popl") == 0)
		write_uint8(0xb0);
	
	write_uint8((reg_name_to_num(args[0] + 1) << 4) | 8);
	return 1;
}

/*
  Codegen function for instructions that have a single
    operand, one byte long, storing two register numbers OR'd together
    
  Supports addl, subl, rrmovl, xorl, andl
*/
int reg_nums_mask_codegen(char *cmd, char **args) {
	if (strcmp(cmd, "addl") == 0)
		write_uint8(0x60);
	else if (strcmp(cmd, "subl") == 0)
		write_uint8(0x61);
	else if (strcmp(cmd, "rrmovl") == 0)
		write_uint8(0x20);
	else if (strcmp(cmd, "xorl") == 0)
		write_uint8(0x63);
	else if (strcmp(cmd, "andl") == 0)
		write_uint8(0x62);
	else if (strcmp(cmd, "multl") == 0)
		write_uint8(0x64);
	else if (strcmp(cmd, "divl") == 0)
		write_uint8(0x65);
	else if (strcmp(cmd, "modl") == 0)
		write_uint8(0x66);
	
	write_uint8((reg_name_to_num(args[0] + 1) << 4) | reg_name_to_num(args[1] + 1));
	return 1;
}

/*
  Codegen function for instructions that have no operands
  Supports halt, ret
*/
int no_operands_codegen(char *cmd, char **args) {
	if (strcmp(cmd, "halt") == 0)
		write_uint8(0x10);   
	else if (strcmp(cmd, "ret") == 0)
		write_uint8(0x90);
	else if (strcmp(cmd, "nop") == 0)
		write_uint8(0x0);
  
	return 1;
}

/*
  Codegen function for instructions that have a 32 bit operand
    that stores the address of a label
    
  Supports je, jle, jmp, jg, jl, jne, jge, call
*/
int label_addr_codegen(char *cmd, char **args) {
	if (strcmp(cmd, "je") == 0)
		write_uint8(0x73);
	else if (strcmp(cmd, "jle") == 0)
		write_uint8(0x71);
	else if (strcmp(cmd, "jmp") == 0)
		write_uint8(0x70);
	else if (strcmp(cmd, "jg") == 0)
		write_uint8(0x76);
	else if (strcmp(cmd, "jl") == 0)
		write_uint8(0x72);
	else if (strcmp(cmd, "jne") == 0)
		write_uint8(0x74);
	else if (strcmp(cmd, "jge") == 0)
		write_uint8(0x75);
	else if (strcmp(cmd, "call") == 0)
		write_uint8(0x80);
  
    Label *label = find_label(args[0]);
	
	if (label == NULL) {
		DBG_PRINT("Invalid label name %s (len = %d)\n", args[0], (int)strlen(args[0]));
		return 0;
	}
	
	DBG_PRINT("Label addr: %d, mem_len = %d\n", label->addr, mem_len);
	write_uint32(label->addr);
	return 1;
}

// Codegen function for irmovl
int irmovl_codegen(char *cmd, char **args) {
	write_uint8(0x30);
	write_uint8(reg_name_to_num(args[1]+1) | 0x80); // | 0x80 for yis/yas compatibility (signifies no register)
	
	Label *label = find_label(args[0]);
	
	if (label != NULL) {
		write_uint32(label->addr);
	} else {
		char *data = args[0];
		if (*data == '$')
			data++;
		
		if (!valid_stol_str(data)) {
			DBG_PRINT("Invalid argument to irmovl %s\n", data);
			return 0;
		}
		
		write_uint32(stol(data));
	}
	
	return 1;
}

// Codegen for .long
int long_codegen(char *cmd, char **args) {
	write_uint32(stol(args[0]));
	return 1;
}

// Codegen for .pos (doesn't actually write any code to memory)
int pos_codegen(char *cmd, char **args) {
	int new_pos = stol(args[0]);
    
	DBG_PRINT("new_pos = %d (str: %s)\n", new_pos, args[0]);
	
	if (new_pos > sizeof(memory))
		return 0;
	
	mem_len = new_pos;
	return 1;
}

// Codegen for .align (doesn't actually write any code to memory)
int align_codegen(char *cmd, char **args) {
	int align_by = stol(args[0]);
	int new_pos = round_up_to_nearest(mem_len, align_by);
	
	if (new_pos > sizeof(memory))
		return 0;
	
	mem_len = new_pos;
	return 1;
}

/*
  Returns the size of an instruction, in bytes
  addr parameter is needed for .pos/.align directives (since their "size" depends on their addr)
*/
int get_instr_size(char *instr_name, uint16 addr) {
	int i;
	
	if (*instr_name == '.') {    
		if (strncmp(instr_name, ".pos", 4) == 0)
			return stol(&instr_name[5]) - addr;
		
		else if (strncmp(instr_name, ".align", 6) == 0) {
			int align_by = stol(&instr_name[7]);
			int new_pos = round_up_to_nearest(addr, align_by);
			return new_pos - addr;
		}
		
		else if (strncmp(instr_name, ".long", 5) == 0)
			return 4;
        
		else {
			return 0;
		}
	} else {
		char *space = strchr(instr_name, ' ');
		int size = 0;
		
		if (space != NULL)
			*space = '\0'; // separate instruction name from parameters
  
		for (i = 0; i < num_instrs; i++) {
			if (strcmp(instrs[i].name, instr_name) == 0) {
				size = instrs[i].size;
				break;
			}
		}
		
		if (space != NULL)
			*space = ' ';
		
		return size;
	}
}

// Builds the program memory
int gen_bytecode(char *filename) {
	FILE *str_in;
	char line_in[4096];
	int i;
	
	mem_len = 0;
	memset(memory, 0, sizeof(memory));
	
	str_in = fopen(filename, "r");
	if (str_in == NULL) {
		DBG_PRINT("Error opening %s for reading\n", filename);
		return INVALID_FILE;
	}
	
	parse_labels(str_in);
	rewind(str_in);
	
	while (read_y86_line(str_in, line_in, sizeof(line_in))) {
		if (!parse_line(line_in)) {
			DBG_PRINT("Error parsing %s\n", line_in);
			return PARSE_ERROR;
		}
		
		DBG_PRINT("\n\n\n");
	}
	
	fclose(str_in);
	
	DBG_PRINT("LABELS =>\n");
	for (i = 0; i < num_labels; i++)
		DBG_PRINT("%s %d\n", labels[i]->name, labels[i]->addr);
	
	return SUCC;
}

// Generates a yis compatible yo file
int gen_yo_file(char *filename) {
	int i;
	FILE *out = fopen(filename, "w+");
	SourceLine *cur_line = source_lines;
	
	if (out == NULL)
		return 0;
	
	while (cur_line != NULL) {
		fprintf(out, "0x%03x: ", cur_line->addr);
		
		if (!is_label_line(cur_line->line) && strncmp(cur_line->line, ".pos", 4) && strncmp(cur_line->line, ".align", 6)) {
			int instr_size = get_instr_size(cur_line->line, cur_line->addr);
      
			for (i = cur_line->addr; i < cur_line->addr + instr_size; i++)
				fprintf(out, "%02x", memory[i]);
			for (i = 0; i < 13 - 2*instr_size; i++)
				fprintf(out, " ");
			fprintf(out, "| ");
			fprintf(out, "%s\n", cur_line->line);
		} else {
			fprintf(out, "             | %s\n", cur_line->line);
		}
		
		cur_line = cur_line->next;
	}
	
	fclose(out);
	return 1;
}

// Returns a registers number (the number, not value)
int reg_name_to_num(char *reg) {
	if (strcmp(reg, "eax") == 0)
		return EAX;
	
	if (strcmp(reg, "ecx") == 0)
		return ECX;
	
	if (strcmp(reg, "edx") == 0)
		return EDX;
	
	if (strcmp(reg, "ebx") == 0)
		return EBX;
	
	if (strcmp(reg, "esi") == 0)
		return ESI;
	
	if (strcmp(reg, "edi") == 0)
		return EDI;
	
	if (strcmp(reg, "esp") == 0)
		return ESP;
	
	if (strcmp(reg, "ebp") == 0)
		return EBP;
	
	return -1;
}
