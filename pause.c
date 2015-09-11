// pause.c - Contains the code necessary to save the state of the simulator/debugger to a file and then to restore it at a later time
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "simulator.h"
#include "assembler.h"
#include "console.h"
#include "debugger.h"

/*
  Writes a string to the file first by writing the size of the string
  to the file (including the null terminator) and then writing the string
  itself (including the null termintor).
*/
static void write_string(FILE *out, char *str) {
	uint16 size;
  
	assert(out != NULL && str != NULL);
  
	size = strlen(str)+1;
	fwrite(&size, 1, sizeof(size), out);
	fwrite(str, 1, size, out);
}

// Reads a string written to the file by write_string
static char *read_string(FILE *in) {
	uint16 size;
	char *str;

	assert(in != NULL);
  
	fread(&size, 1, sizeof(size), in);
	str = malloc(size);

	if (str != NULL)
		fread(str, 1, size, in);

	return str;
}

/*
  Writes a Condition to the file by first writing the value descriptor strings
  and then writing the relational operator
*/
void write_condition(FILE *out, Condition *con) {
	assert(out != NULL && con != NULL);

	write_string(out, con->x);
	write_string(out, con->y);
	fwrite(&con->op, 1, sizeof(con->op), out);
}

// Reads a condition written to the file by write_condition
Condition *read_condition(FILE *in) {
	Condition *con = malloc(sizeof(Condition));

	assert(in != NULL);
  
	if (con == NULL)
		return NULL;
  
	con->x = read_string(in);
	con->y = read_string(in);
	fread(&con->op, 1, sizeof(con->op), in);

	return con;
}

// Recursive helper function for write_condition_list
void write_condition_list_rec(FILE *out, ConditionList *cur) {
	if (cur != NULL) {
		write_condition(out, cur->con);
		write_condition_list_rec(out, cur->next);
	}
}

/*
  Writes a condition list to the file.
  First writes the size of the list as a 2 byte integer,
  Then it writes each Condition node in the linked list by calling write_condition
*/
void write_condition_list(FILE *out, ConditionList *list) {
	uint16 size;

	size = get_cond_list_size(list);
	fwrite(&size, 1, sizeof(size), out);
	write_condition_list_rec(out, list);
}

/*
  Reads a single Condition node, from a linked list,
  written to the file by write_condition
  Returns the new ConditionList node or NULL if an error occured
*/
ConditionList *read_condition_list_node(FILE *in) {
	ConditionList *new_node = malloc(sizeof(ConditionList));

	if (new_node == NULL)
		return NULL;

	new_node->next = NULL;
	new_node->con = read_condition(in);

	if (new_node->con == NULL) {
		free(new_node);
		return NULL;
	}
  
	return new_node;
}

/*
  Reads an entire condition linked list written to the file by write_condition_list
  Builds the linked list and returns it, returns NULL when the list size was 0
*/
ConditionList *read_condition_list(FILE *in) {
	int i;
	uint16 size;
	ConditionList *new_list = NULL, *cur = NULL, *prev = NULL;

	assert(in != NULL);
  
	fread(&size, 1, sizeof(size), in);
 
	for (i = 0; i < size; i++) {
		cur = read_condition_list_node(in);
    
		if (prev == NULL)
			new_list = cur;
		else
			prev->next = cur;

		prev = cur;
	}

	return new_list;
}

// Writes a single SourceLine node from the linked list to a file  
void write_source_line_node(FILE *out, SourceLine *line) {
	write_string(out, line->line);

	fwrite(&line->addr, 1, sizeof(line->addr), out);
	fwrite(&line->has_breakpoint, 1, sizeof(line->has_breakpoint), out);
	fwrite(&line->has_cond_breakpoint, 1, sizeof(line->has_cond_breakpoint), out);

	write_condition_list(out, line->cond_bp_list);
}

// Reads a SourceLine node written to the file by write_source_line_node
SourceLine *read_source_line_node(FILE *in) {
	SourceLine *new_node = malloc(sizeof(SourceLine));
  
	if (new_node == NULL)
		return NULL;

	new_node->next = NULL;
	new_node->line = read_string(in);
  
	fread(&new_node->addr, 1, sizeof(new_node->addr), in);
	fread(&new_node->has_breakpoint, 1, sizeof(new_node->has_breakpoint), in);
	fread(&new_node->has_cond_breakpoint, 1, sizeof(new_node->has_cond_breakpoint), in);

	new_node->cond_bp_list = read_condition_list(in);
  
	return new_node;
}

// Recursive helper function for write_source_lines 
void write_source_lines_rec(FILE *out, SourceLine *cur) {
	if (cur != NULL) {
		write_source_line_node(out, cur);
		write_source_lines_rec(out, cur->next);
	}
}

// Writes the SourceLine linked list to the file, similar to write_condition_list
void write_source_lines(FILE *out, SourceLine *list) {
	uint16 size;

	assert(out != NULL);
  
	size = get_source_lines_size(list);
	fwrite(&size, 1, sizeof(size), out);
	write_source_lines_rec(out, list);
}

// Reads an entire SourceLine linked list written to the file by write_source_lines 
SourceLine *read_source_lines(FILE *in) {
	int i;
	uint16 size;
	SourceLine *new_list = NULL, *cur = NULL, *prev = NULL;

	assert(in != NULL);
  
	fread(&size, 1, sizeof(size), in);

	for (i = 0; i < size; i++) {
		cur = read_source_line_node(in);

		if (prev == NULL)
			new_list = cur;
		else
			prev->next = cur;
    
		prev = cur;
	}

	return new_list;
}

/*
  Saves the state of the simulator, debugger, and console to a file.
  For more information about the format of the file read the simulator overview.
*/
int gen_pause_file(char *filename) {
	int i;
	FILE *f_out;
	uint16 cur_PC = sim_get_pc();

	assert(filename != NULL);

	f_out = fopen(filename, "w+");
  
	if (f_out == NULL)
		return 0;
  
	fwrite(registers, 1, sizeof(registers), f_out);
	fwrite(&cur_PC, 1, sizeof(cur_PC), f_out);
	fwrite(&flgs, 1, sizeof(Flags), f_out);
	fwrite(memory, 1, sizeof(memory), f_out);
	write_condition_list(f_out, watch_conditions);
	write_source_lines(f_out, source_lines);

	fwrite(&dbg_win_frac, 1, sizeof(dbg_win_frac), f_out);
	fwrite(&num_lines, 1, sizeof(num_lines), f_out);
	fwrite(&cur_sim_line, 1, sizeof(cur_sim_line), f_out);
	fwrite(&next_dbg_line, 1, sizeof(next_dbg_line), f_out);
	fwrite(&sim_window_overflow, 1, sizeof(sim_window_overflow), f_out);
	fwrite(sim_title, 1, 512, f_out);
	fwrite(dbg_title, 1, 512, f_out);
	fwrite(&line_width, 1, sizeof(line_width), f_out);
  
	fwrite(&num_dbg_lines, 1, sizeof(num_dbg_lines), f_out);
	fwrite(&num_sim_lines, 1, sizeof(num_sim_lines), f_out);
   
	for (i = 0; i < num_dbg_lines; i++)
		fwrite(dbg_lines[i], 1, line_width+1, f_out);

	for (i = 0; i < num_sim_lines; i++)
		fwrite(sim_lines[i], 1, line_width+1, f_out);
  
	fclose(f_out);
	return 1;
}

// Restores the state of the simulator saved to the pause file by gen_pause_file
int restore_simulator_state(char *pause_file) {
	int i, x, y;
	FILE *f_in;
	uint16 new_PC;

	assert(pause_file != NULL);

	f_in = fopen(pause_file, "r");
  
	if (f_in == NULL) {
		write_to_dbg("Error opening file for reading");
		return 0;
	}
  
	fread(registers, 1, sizeof(registers), f_in);
	fread(&new_PC, 1, sizeof(uint16), f_in);
	sim_set_pc(new_PC);
  
	fread(&flgs, 1, sizeof(Flags), f_in);
	fread(memory, 1, sizeof(memory), f_in);
  
	// TODO: free source_lines if read_source_lines fails (store ret. value in a variable)

	free_condition_list(watch_conditions);
  
	watch_conditions = read_condition_list(f_in);

	SourceLine *new_source_lines = read_source_lines(f_in);
	SourceLine *cur_line_old = source_lines;
	SourceLine *cur_line_new = new_source_lines;
	int same_source_lines = 1;
  
	while (cur_line_old != NULL && cur_line_new != NULL) {
		if (strcmp(cur_line_old->line, cur_line_new->line)) {
			DBG_PRINT("Source lines err: %s != %s\n", cur_line_old->line, cur_line_new->line);
			same_source_lines = 0;
			break;
		}

		cur_line_old = cur_line_old->next;
		cur_line_new = cur_line_new->next;
	}

	if ((cur_line_old == NULL && cur_line_new != NULL) ||
		(cur_line_old != NULL && cur_line_new == NULL)) { // checking if source_lines is a different size
		DBG_PRINT("Source lines different size\n");
		same_source_lines = 0;
	}

	if (!same_source_lines) {
		fclose(f_in);
		write_to_dbg("Error trying to restore from %s (different source file)", pause_file);
		return 0;
	}
  
	source_lines = new_source_lines;

	free_dbg_and_sim_lines();
	
	fread(&dbg_win_frac, 1, sizeof(dbg_win_frac), f_in);
	fread(&num_lines, 1, sizeof(num_lines), f_in);
	fread(&cur_sim_line, 1, sizeof(cur_sim_line), f_in);
	fread(&next_dbg_line, 1, sizeof(next_dbg_line), f_in);
	fread(&sim_window_overflow, 1, sizeof(sim_window_overflow), f_in);
	fread(sim_title, 1, 512, f_in);
	fread(dbg_title, 1, 512, f_in);
	fread(&line_width, 1, sizeof(line_width), f_in);
  
	fread(&num_dbg_lines, 1, sizeof(num_dbg_lines), f_in);
	fread(&num_sim_lines, 1, sizeof(num_sim_lines), f_in);

	getmaxyx(stdscr, y, x);

	// this console isn't big enough :(
	if (line_width > x || num_lines > y) {
		delwin(sim);
		delwin(dbg);
		endwin();
		destroy_dbg_print();
		printf("Original window too big to fit in this console\n");
		fclose(f_in);
		exit(0);
	}
  
	dbg_lines = malloc(sizeof(char*) * num_dbg_lines);
	for (i = 0; i < num_dbg_lines; i++) {
		dbg_lines[i] = malloc(line_width+1);
		memset(dbg_lines[i], 0, line_width+1);
	}
  
	sim_lines = malloc(sizeof(char*) * num_sim_lines);
	for (i = 0; i < num_sim_lines; i++) {
		sim_lines[i] = malloc(line_width+1);
		memset(sim_lines[i], 0, line_width+1);
	}
  
	for (i = 0; i < num_dbg_lines; i++)
		fread(dbg_lines[i], 1, line_width+1, f_in);
  
	for (i = 0; i < num_sim_lines; i++)
		fread(sim_lines[i], 1, line_width+1, f_in);

	redraw_window(sim);
	redraw_window(dbg);  
	sim_exec_bytecode();
  
	fclose(f_in);
	return 1;
}
