#ifndef CONDITION_H
#define CONDITION_H

#include "common.h"

typedef enum {
	OP_L, /* < */
	OP_G, /* > */
	OP_EQ, /* = */
	OP_GEQ, /* >= */
	OP_LEQ, /* <= */
	OP_NEQ, /* != */
} Op;

typedef struct _Condition {
	char *x, *y; // x and y are value descriptors
	Op op; 
} Condition;

typedef struct _ConditionList {
	Condition *con;
	struct _ConditionList *next;
} ConditionList;

uint32 calc_value_descriptor(char *val_desc, int *error);
Condition *find_true_condition_in_list(ConditionList *list);
int add_condition_list(ConditionList **list, Condition *cond);
int delete_condition_list(ConditionList **list, Condition *cond);
Condition *find_cond_by_expr(ConditionList *list, char *expr);
void free_condition_list(ConditionList *list);
int build_cond_by_expr(Condition *cond, char *expr);
int get_cond_list_size(ConditionList *list);
int add_condition_list(ConditionList **list, Condition *cond);
int remove_condition_list(ConditionList **list, Condition *cond);
Condition *find_cond_by_expr(ConditionList *list, char *expr);
Condition *find_true_condition_in_list(ConditionList *list);

#endif
