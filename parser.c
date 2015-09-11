// parser.c - Contains code to parse y86 source files and code to store each source line in a linked list
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "common.h"
#include "simulator.h"
#include "assembler.h"
#include "debugger.h"
#include "parser.h"
#include "console.h"

Label **labels = NULL;
int num_labels = 0;

SourceLine *source_lines = NULL;

/*
  Takes as input a line, read by read_y86_line, and calls the necessary codegen function
   in assembler.c to build the program memory
   
  Returns 1 on success, 0 on error
*/
int parse_line(char *line) {
	char *space, *cmd, *rest, *args[8], *line_copy;
	int i, num_args = 0, succ = 1;
  
	assert(line != NULL);
  
	DBG_PRINT("Line: %s\n", line);
	
	line_copy = strdup(line);
	
	if (line_copy == NULL)
		return 0;
	
	if (!str_ends_with(line, ':')) {
		// this is not a label line
		space = strchr(line_copy, ' ');
		
		for (i = 0; i < 8; i++)
			args[i] = NULL;
		
		if (space != NULL) {
			*space = '\0'; // separate instruction from parameters  
			cmd = line_copy;
			rest = cmd + strlen(cmd) + 1;
			
			num_args = 1;
			args[0] = strtok(rest, ",");
			args[1] = strtok(NULL, ",");
			
			if (args[1] != NULL)
				num_args++;
		} else {
			cmd = line_copy;
		}

		DBG_PRINT("Instruction: %s, num_args=%d\n", cmd, num_args);
		
		if (strcmp(cmd, "irmovl") == 0) {
			succ = (num_args != 2) ? 0 : irmovl_codegen(cmd, args);
		}
		
		// rmmovl %eax, (%ebx) is equiv. to rmmovl %eax, 0(%ebx)
		else if (strcmp(cmd, "rmmovl") == 0 || strcmp(cmd, "mrmovl") == 0) {
			succ = (num_args != 2) ? 0 : reg_mem_codegen(cmd, args);
		}
		
		else if (strcmp(cmd, "rdint") == 0 || strcmp(cmd, "rdch") == 0 || strcmp(cmd, "wrch") == 0 || strcmp(cmd, "wrint") == 0 ||
				 strcmp(cmd, "pushl") == 0 || strcmp(cmd, "popl") == 0) {
			succ = (num_args != 1) ? 0 : reg_num_codegen(cmd, args);
		}
		
		else if (strcmp(cmd, "addl") == 0 || strcmp(cmd, "subl") == 0 || strcmp(cmd, "xorl") == 0 || strcmp(cmd, "andl") == 0 ||
				 strcmp(cmd, "multl") == 0 || strcmp(cmd, "divl") == 0 || strcmp(cmd, "modl") == 0 || strcmp(cmd, "rrmovl") == 0) {
			succ = (num_args != 2) ? 0 : reg_nums_mask_codegen(cmd, args);
		}
		
		else if (strcmp(cmd, "halt") == 0 || strcmp(cmd, "ret") == 0 || strcmp(cmd, "nop") == 0) {
			succ = (num_args != 0) ? 0 : no_operands_codegen(cmd, args); 
		}
		
		else if (strcmp(cmd, "je") == 0 || strcmp(cmd, "jle") == 0 || strcmp(cmd, "jmp") == 0 || strcmp(cmd, "jg") == 0 ||
				 strcmp(cmd, "jl") == 0 || strcmp(cmd, "jne") == 0 || strcmp(cmd, "jge") == 0 || strcmp(cmd, "call") == 0) {
		    succ = (num_args != 1) ? 0 : label_addr_codegen(cmd, args);
		}
		
		else if (strcmp(cmd, ".long") == 0) {
			succ = (num_args != 1) ? 0 : long_codegen(cmd, args);
		}
  
		else if (strcmp(cmd, ".pos") == 0) {
			succ = (num_args != 1) ? 0 : pos_codegen(cmd, args);
		}
		
		else if (strcmp(cmd, ".align") == 0) {
		    succ = (num_args != 1) ? 0 : align_codegen(cmd, args);
		}
	}
	
	if (!succ) {
		write_to_dbg("parser: Error processing %s", line);
		get_key_and_exit();
	}
	
	free(line_copy);
	return 1;
}

/*
  Runs through the y86 file, and assign an address to each label
  Also stores each source line in source_lines by calling
    add_source_line (this just happens to be the easiest place to do so)
*/
void parse_labels(FILE *str_in) {
	char line[4096];
	int len, cur_addr = 0;
	Label *cur_label = NULL;
	
	assert(str_in != NULL);
	
	while (read_y86_line(str_in, line, sizeof(line))) {
		if (cur_addr > 4096) {
			DBG_PRINT("cur_addr exceeds 4096\n");
			exit(0);
		}
		
		len = strlen(line);
   
		DBG_PRINT("Read in line: %s\n", line);
        
		if (len == 0)
			continue; /* blank line or line with only a comment, so skip */

		add_source_line(line, cur_addr);    
    
		if (str_ends_with(line, ':')) {
			// this is a label line
			line[len-1] = '\0';
			cur_label = malloc(sizeof(Label));
			strcpy(cur_label->name, line);
			cur_label->addr = cur_addr;

			DBG_PRINT("Got a label line %s at address %x\n", line, cur_addr);

			labels = realloc(labels, (num_labels + 1) * sizeof(Label));
			labels[num_labels++] = cur_label;
		} else {
			// this is not a label line, but we still need to keep counting the current address so we know where we are
			DBG_PRINT("cur instr size = %d, cur addr = %x\n", get_instr_size(line, cur_addr), cur_addr);
			cur_addr += get_instr_size(line, cur_addr); /* line now contains only the instruction */
		}
	}
}

/* Check if a line is a label, returns 1 if it is, and 0 if not */
int is_label_line(char *line) {
	int i;
	char *p, *colon;
	char *alphanumeric;
	
	assert(line != NULL);
	
	colon = strchr(line, ':');
	
	/* should have a : marking the end of the label name */
	if (colon == NULL)
		return 0;
	
	/* only characters that should come after the colon are spaces */
	for (i = 1; i < strlen(colon); i++)
		if (!is_whitespace(colon[i]))
			return 0;

	/* and everything before it should be alphanumeric or a space or underscore */
	for (p = line; p < colon; p++)
		if (!is_alphanumeric(*p) && !is_whitespace(*p) && *p != '_')
			return 0;

	/* ...but we cannot have a space in the middle of the label (e.g. "LA BEL:" disallowed)
	   AND we must have atleast one alphanumeric character for the label name */
	alphanumeric = get_first_alphanumeric(line);
	if (alphanumeric == NULL)
		return 0;
	
	/* from the first alphanumeric character to the : */
	for (p = alphanumeric; p < colon; p++) {
		if (!is_alphanumeric(*p) && *p != '_') /* we should only have alphanumeric characters or underscores */
			return 0;
		p++;
	}
	
	return 1;
}

// Read the next raw line of the file
int read_line(FILE *file, char *buf, int size) {
	int len;
	char *ret;
	
	if (buf == NULL)
		return 0;
	
	memset(buf, 0, size);
	ret = fgets(buf, size, file);
	
	if (ret == NULL)
		return 0;
	
	len = strlen(buf);
	buf[len-1] = '\0';
	
	DBG_PRINT("Read: %s\n", buf);
	return 1;
}

/*
  Reads the next raw line from the file, and puts it into a nice format to parse:
   -> comments are removed from the line
   -> the instruction is converted to <INSTRUCTION NAME> <ARG1>,<ARG2>
        e.g. "  irmovl  $3,   %edx" is convered to "irmovl $3,%edx"
	
   Returns 0 on error, 1 on success
*/
int read_y86_line(FILE *file, char *buf, int size) {
	int i, err, buf_idx = 0;
	char *line_in = malloc(size);
	char *orig_line = line_in;
	
	if (line_in == NULL) {
		DBG_PRINT("malloc fail\n");
		return 0;
	}
	
	memset(buf, 0, size);
	err = read_line(file, line_in, size);
	
	DBG_PRINT("Read in: %s\n", line_in);
	
	if (err == 0) {
		free(orig_line);
		return 0;
	}
	
	/* skip all spaces */
	while (is_whitespace(*line_in))
		line_in++;
	
	/* check if we have a comment in the line, and if so, remove everything after (and including) the # */
	char *comment = strchr(line_in, '#');
	if (comment != NULL)
		*comment = '\0';
	
	/* whole line is a comment */
	if (*line_in == '\0') {
		free(orig_line);
		return 1;
	}
	
	/* check if we have an instruction */
	char *instr_start = NULL, *instr_end = NULL;
	
	for (i = 0; i < num_instrs; i++) {
		int instr_len = strlen(instrs[i].name);
		if (memcmp(line_in, instrs[i].name, instr_len) == 0) {
			if (instr_len > size-1) { /* probably shouldn't happen but just to be safe */
				free(orig_line);
				return 0;
			}
			
			if (is_whitespace(line_in[instr_len]) || line_in[instr_len] == '\0') {
				instr_start = line_in;
				instr_end = instr_start + strlen(instrs[i].name);
				if (instrs[i].num_args > 0)
					instr_end += 1; /* add space to end to make room for arguments */
				break;
			}
		}
	}
    
	/* if this line is an instruction */
	if (instr_start != NULL) {
		// first copy the actual instruction name
		char *params = instr_end;
		strncpy(buf, instr_start, instr_end-instr_start);
		buf_idx += (instr_end-instr_start);
		
		/* and then copy all of the non-spaces after it (i.e. arguments) */
		while (*params != '\0') {
			if (!is_whitespace(*params))
				buf[buf_idx++] = *params;
			params++;
		}
		
		buf[buf_idx] = '\0';
		free(orig_line);
		return 1;
	}

	/* otherwise if we have a label */
	else if (is_label_line(line_in)) {
		char *alphanumeric, *colon, *p;
		
		alphanumeric = get_first_alphanumeric(line_in);
		colon = strchr(line_in, ':');
		for (p = alphanumeric; p <= colon; p++) /* then copy only the "label:" part of the label (w/o any spaces) */ 
			buf[buf_idx++] = *p;
		
		free(orig_line);
		return 1;
	}
	
	else if (strncmp(line_in, ".long", 5) == 0 || strncmp(line_in, ".pos", 4) == 0 || strncmp(line_in, ".align", 6) == 0) {
		if (strncmp(line_in, ".long", 5) == 0)
			strcpy(buf, ".long ");
		else if (strncmp(line_in, ".pos", 4) == 0)
			strcpy(buf, ".pos ");
		else
			strcpy(buf, ".align ");

		char *arg_data = &line_in[strlen(buf)];

		// skip the spaces in between .<directive> and value
		while (*arg_data == ' ')
			arg_data++;

    	buf_idx = strlen(buf);
		
		while (*arg_data != ' ' && *arg_data != '\0') {
			buf[buf_idx++] = *arg_data;
			arg_data++;
		}
		
		buf[buf_idx] = '\0';
		free(orig_line);
		return 1;
	}
	
	DBG_PRINT("No match for %s, returning 0\n", line_in);
	free(orig_line);
	return 0;
}

// Checks if the SourceLine is a line with an instruction on it
static int is_instruction(SourceLine *line) {
	int i;
	char *space, *instr_name = NULL;
	int alloced_name = 0, is_instr = 0;
	
	if (line == NULL)
		return 0;
  
	space = strchr(line->line, ' ');
  
	if (space == NULL)
		instr_name = line->line;
	else {
		instr_name = malloc(space - line->line + 1);
		alloced_name = 1;
		strncpy(instr_name, line->line, space - line->line);
		instr_name[space - line->line] = '\0';
	}
  
	DBG_PRINT("name=%s\n", instr_name);
  
	for (i = 0; i < num_instrs; i++) {
		if (strcmp(instrs[i].name, instr_name) == 0) {
			is_instr = 1;
			break;
		}
	}

	if (alloced_name)
		free(instr_name);
  
	return is_instr;
}

// Return the size of a SourceLine linked list
int get_source_lines_size(SourceLine *lines) {
	int size = 0;
  
	while (lines != NULL) {
		size++;
		lines = lines->next;
	}

	return size;
}

// Adds a node to the linked list of source lines
int add_source_line(char *line, int addr) {
	SourceLine *new_line = malloc(sizeof(SourceLine));

	if (new_line == NULL)
		return 0;
  
	new_line->line = malloc(strlen(line)+1);

	if (new_line->line == NULL) {
		free(new_line);
		return 0;
	}
  
	strcpy(new_line->line, line);
	new_line->addr = addr;
	new_line->next = NULL;
	new_line->has_breakpoint = 0;
	new_line->has_cond_breakpoint = 0;
	new_line->cond_bp_list = NULL;
  
	if (source_lines == NULL) {
		source_lines = new_line;
	} else {
		SourceLine *last = source_lines;

		while (last->next != NULL)
			last = last->next;

		last->next = new_line;
	}

	return 1;
}

/*
  Finds a node in the linked list of source lines, returning NULL if it is not present
  Multiple source lines may have this addr, but only ones with instructions and/or .long declarations
  are returned, not the label name
*/
SourceLine *find_source_line(uint16 addr) {
	SourceLine *cur = source_lines;

	while (cur != NULL) {
		if (cur->addr == addr &&
			(is_instruction(cur) || strncmp(cur->line, ".long", 5) == 0))
			return cur;
    
		cur = cur->next;
	}

	return NULL;
}

// Find a label by name, and return it, or NULL if it does not exist
Label *find_label(char *name) {
	int i;
  
	for (i = 0; i < num_labels; i++)
		if (strcmp(labels[i]->name, name) == 0)
			return labels[i];
  
	return NULL;
}

/*
  Search the array of labels for the label at the specified address
  Returns the label if it is present in the array, and NULL if not
*/
Label *find_label_by_addr(uint16 addr) {
	int i;
  
	for (i = 0; i < num_labels; i++)
		if (labels[i]->addr == addr)
			return labels[i];
  
	return NULL;
}
