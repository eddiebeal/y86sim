// condition.c - Consits of code to represent a condition, used for conditional breakpoints and watch conditions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "common.h"
#include "console.h"
#include "debugger.h"
#include "simulator.h"
#include "condition.h"

// Returns 1 if val_desc is a valid value descriptor and 0 if it is not
int valid_val_desc(char *val_desc) {
	int err;
	calc_value_descriptor(val_desc, &err);
	return err == SUCC;
}

/*
  Takes an expression as a string and builds a struct Condition.
  Returns SUCC, INVALID_COND_EXPR or MEM_ERR
*/
int build_cond_by_expr(Condition *cond, char *expr) {
	char *lt, *gt, *eq, *neq, *op_str_ptr;
	char *expr_copy;

	assert(cond != NULL);

	if (expr == NULL)
		return INVALID_COND_EXPR;
  
	expr_copy = strdup(expr);

	if (expr_copy == 0)
		return MEM_ERR;
  
	memset(cond, 0, sizeof(Condition));

	lt = strstr(expr_copy, "<");
	gt = strstr(expr_copy, ">");
	neq = strstr(expr_copy, "!");
	eq = strstr(expr_copy, "=");

	if (lt != NULL) {
		if (lt+1==eq) {
			cond->op = OP_LEQ;
		} else {
			cond->op = OP_L;
		}
		
		op_str_ptr = lt;
	}
  
	else if (gt != NULL) {
		if (gt+1==eq) {
			cond->op = OP_GEQ;
		} else {
			cond->op = OP_G;
		}
		
		op_str_ptr = gt;
	}
  
	else if (neq != NULL) {
		cond->op = OP_NEQ;
		op_str_ptr = neq;
	}

	// eq must be checked last (since it appears in the other operators)
	else if (eq != NULL) {
		cond->op = OP_EQ;
		op_str_ptr = eq;
	}
    
	else {
		free(expr_copy);
		return INVALID_COND_EXPR;
	}
	
	char *x_val_desc = expr_copy, *y_val_desc = op_str_ptr+1;
	
	// split the string into two parts, the x val desc and the y val descriptor
	*op_str_ptr = '\0';
	if (cond->op >= OP_GEQ) {
		*(op_str_ptr+1) = '\0';
		y_val_desc++;
	}
	
	if (!valid_val_desc(x_val_desc) || !valid_val_desc(y_val_desc)) {
		free(expr_copy);
		return INVALID_COND_EXPR;
	}
	
	cond->x = malloc(strlen(x_val_desc)+1);
	cond->y = malloc(strlen(y_val_desc)+1);
	strcpy(cond->x, x_val_desc);
	strcpy(cond->y, y_val_desc);

	free(expr_copy);
	return SUCC;
}

/*
  Computes the value of the value descriptor val_desc
  Returns the value of the value descriptor, or 0 on error
  Since 0 is a valid value descriptor, a pointer to an integer variable error is accepted
  If error is not NULL, it will be set to either SUCC or INVALID_VAL_DESC
*/
uint32 calc_value_descriptor(char *val_desc, int *error) {
	DBG_PRINT("val_desc=%s\n", val_desc);
	
	if (*val_desc == '%') {
		DBG_PRINT("Register value\n");
		
		int reg_num = reg_name_to_num(val_desc+1);
		if (reg_num == -1) {
			if (error != NULL)
				*error = INVALID_VAL_DESC;
			return 0;
		}
		
		DBG_PRINT("Register number = %d, val = %d\n", reg_num, registers[reg_num]);
		
		if (error != NULL)
			*error = SUCC;
		
		return registers[reg_num];
	}

	else if (valid_stol_str(val_desc) || (*val_desc == '$' && valid_stol_str(val_desc+1))) {
		DBG_PRINT("Constant value, stol(val_desc) = %d\n", (int)stol(val_desc));

		if (error != NULL)
			*error = SUCC;
		
		if (valid_stol_str(val_desc))
			return stol(val_desc);
		else
			return stol(val_desc+1);
	}

	else if (*val_desc == '[' && str_ends_with(val_desc, ']')) {
		char *addr_str, *num_bytes_str, *comma, *closing;
		uint32 addr, num_bytes;
		uint32 ret = 0;
		int error_code = SUCC;

		DBG_PRINT("Memory value\n");

		comma = strchr(val_desc, ',');
		closing = strchr(val_desc, ']');
        
		if (char_count(val_desc, ',') > 1 || char_count(val_desc, '[') > 1 ||
			char_count(val_desc, ']') > 1) {
			DBG_PRINT("Should be only one comma, one ], and one [\n");
			error_code = INVALID_VAL_DESC;
			goto done;
		}
    
		if (comma == NULL || closing == NULL) {
			DBG_PRINT("Missing a comma\n"); // checking for closing NULL just to be safe, shouldn't happen
			error_code = INVALID_VAL_DESC;
			goto done;
		}
		
		addr_str = val_desc+1;
		num_bytes_str = comma+1;
		
		*comma = '\0'; // avoid storing in another buffer
		*closing = '\0';
		
		if (!valid_stol_str(addr_str) || !valid_stol_str(num_bytes_str)) {
			error_code = INVALID_VAL_DESC;
			goto done;
		}
    
		addr = stol(addr_str);
		num_bytes = stol(num_bytes_str);
		
		/*
		  Base cases:
		  can read 1 byte from 4095
		  can read 2 bytes from 4094
		  can read 4 bytes from 4091
		*/
		if (addr > 4096 - num_bytes) {
			error_code = INVALID_VAL_DESC;
			goto done; // read would overflow
		}
		
		DBG_PRINT("addr=0x%x, num_bytes=0x%x\n", addr, num_bytes);
		
		if (num_bytes == 1)
			ret = memory[addr];
		else if (num_bytes == 2)
			ret = *((uint16*)&memory[addr]);
		else if (num_bytes == 4)
			ret = *((uint32*)&memory[addr]);
		
	done:
		// restore string
		*comma = ',';
		*closing = ']';
		
		if (error != NULL)
			*error = error_code;
		
		return ret;
	}
	
	if (error != NULL)
		*error = INVALID_VAL_DESC;
	
	return 0;
}

// Computes the size of the condition linked list
int get_cond_list_size(ConditionList *list) {
	int size = 0;
	
	while (list != NULL) {
		size++;
		list = list->next;
	}
	
	return size;
}

/*
  Adds a condition to the linked list
  Returns 0 on error and 1 on success
*/
int add_condition_list(ConditionList **list, Condition *cond) {
	ConditionList *new_entry = malloc(sizeof(ConditionList));
	
	if (new_entry == NULL || list == NULL)
		return 0;
  
	new_entry->con = cond;
	new_entry->next = NULL;
	
	if (*list == NULL)
		*list = new_entry;
	else {
		new_entry->next = *list;
		*list = new_entry;
	}
	
	return 1;
}

// Removes a condition from the linked list
int remove_condition_list(ConditionList **list, Condition *cond) {
	ConditionList *prev = NULL, *cur;
	int found = 0;
	
	if (list == NULL || cond == NULL)
		return 0;

	cur = *list;
	
	while (cur != NULL) {
		if (strcmp(cur->con->x, cond->x) == 0 &&
			strcmp(cur->con->y, cond->y) == 0 &&
			cur->con->op == cond->op) {
			found = 1;
			break;
		}
		
		prev = cur;
		cur = cur->next;
	}
	
	if (!found)
		return 0;
	
	if (prev == NULL) {
		*list = (*(list))->next;
	} else {
		prev->next = cur->next;
	}
	
	if (cur->con != NULL)
		free(cur->con);
	free(cur);
	
	return 1;
}

// Searches the linked list for a conditional expression supplied as a string
Condition *find_cond_by_expr(ConditionList *list, char *expr) {
	Condition *cond = malloc(sizeof(Condition));
	ConditionList *cur;
	Condition *cond_in_list = NULL;
  
	if (build_cond_by_expr(cond, expr) != SUCC) {
		free(cond);
		return NULL;
	}
	
	cur = list;
	
	while (cur != NULL) {
		if (strcmp(cur->con->x, cond->x) == 0 &&
			strcmp(cur->con->y, cond->y) == 0 &&
			cur->con->op == cond->op) {
			cond_in_list = cur->con;
			break;
		}
		
		cur = cur->next;
	}

	free(cond->x);
	free(cond->y);
	free(cond);
	
	return cond_in_list;
}

// Returns 1 if the condition currently holds, and 0 if not
int condition_holds(Condition *cond) {
	int x_err, y_err, x_desc_val, y_desc_val;
  
	assert(cond != NULL);
	
	x_desc_val = calc_value_descriptor(cond->x, &x_err);
	y_desc_val = calc_value_descriptor(cond->y, &y_err);
	
	if (x_err != SUCC || y_err != SUCC)
		return -1;
	
	switch (cond->op) {
	case OP_L:
		return x_desc_val < y_desc_val;
	case OP_G:
		return x_desc_val > y_desc_val;
	case OP_EQ:
		return x_desc_val == y_desc_val;
	case OP_GEQ:
		return x_desc_val >= y_desc_val;
	case OP_LEQ:
		return x_desc_val <= y_desc_val;
	case OP_NEQ:
		return x_desc_val != y_desc_val;
	}
	
	return -1;
}

/*
  Searches the linked list for the first condition that (currently) holds
  Returns the condition if one is found and NULL otherwise
*/
Condition *find_true_condition_in_list(ConditionList *list) {
	ConditionList *cur_cond = list;
	
	while (cur_cond != NULL) {
		if (condition_holds(cur_cond->con))
			return cur_cond->con;
		
		cur_cond = cur_cond->next;
	}
	
	return NULL;
}

// Frees the condition, including the value descriptors x and y
void free_condition(Condition *cond) {
	if (cond != NULL) {
		if (cond->x != NULL)
			free(cond->x);
		if (cond->y != NULL)
			free(cond->y);
		free(cond);
	}
}

// Frees all conditions in a linked list of conditions
void free_condition_list(ConditionList *list) {
	if (list != NULL) {
		ConditionList *cur = list, *next = NULL;
		
		while (cur != NULL) {
			free_condition(cur->con);
			next = cur->next;
			free(cur);
			cur = next;
		}
	}
}
