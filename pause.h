#ifndef PAUSE_H
#define PAUSE_H

#include "debugger.h"

void write_condition_list(FILE *out, ConditionList *list);
ConditionList *read_condition_list(FILE *in);
SourceLine *read_source_lines(FILE *in);
int gen_pause_file(char *filename);
int restore_simulator_state(char *pause_file);

#endif
