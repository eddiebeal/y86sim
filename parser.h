#ifndef PARSER_H
#define PARSER_H
#include "common.h"
#define MAX_LABEL_NAME 32

typedef struct label {
	char name[MAX_LABEL_NAME];
	uint16 addr;
} Label;

extern Label **labels;
extern int num_labels;

typedef struct _SourceLine {
	char *line;
	uint16 addr;
	uint8 has_breakpoint;
	uint8 has_cond_breakpoint;
	ConditionList *cond_bp_list;
	struct _SourceLine *next;
} SourceLine;

extern SourceLine *source_lines;

int parse_line(char *line);
void parse_labels(FILE *str_in);
int get_source_lines_size(SourceLine *lines);
SourceLine *find_source_line(uint16 addr);
Label *find_label(char*);
Label *find_label_by_addr(uint16);
int reg_name_to_num(char *reg);
int is_label_line(char *line);
int read_y86_line(FILE *file, char *buf, int size);
int add_source_line(char *line, int addr);

#endif
