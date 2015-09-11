#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "common.h"
#include "condition.h"

extern int dbg_step;
extern ConditionList *watch_conditions;

int dbg_suspend_check();
void dbg_suspend_program();
void print_condition_list(char *list_title, ConditionList *list);

#endif
