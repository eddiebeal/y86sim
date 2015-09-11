// debugger.c - Consists of code to check if the debugger should be activated and code to activate it
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "assembler.h"
#include "common.h"
#include "console.h"
#include "debugger.h"
#include "simulator.h"
#include "condition.h"
#include "pause.h"

static void switch_to_debugger(char *title, ...);
static void print_labels();
static void print_source();

int dbg_step = 0;
ConditionList *watch_conditions = NULL;

/*
  Called by simulator before executing each instruction.
  Returns 1 if we should suspend execution to transfer control
  to the debugger, and 0 if not. We transfer control to the debugger
  when any of the following conditions hold:
     1) The next instruction to execute has a (unconditional) breakpoint
     2) The next instruction to execute has a conditional breakpoint,
           and that condition is met
     3) Any of the watch conditions are met
     4) The user has requested suspension due to a step command in the debugger
*/
int dbg_suspend_check() {
	SourceLine *line = find_source_line(sim_get_pc());
	
	if (line == NULL)
		return 0;
	
	return dbg_step == 1 ||
		line->has_breakpoint ||
		find_true_condition_in_list(line->cond_bp_list) != NULL ||
		find_true_condition_in_list(watch_conditions) != NULL;
}

// Called by the simulator to transfer control to the debugger
void dbg_suspend_program() {
	int PC = sim_get_pc();
	SourceLine *line = find_source_line(PC);
	
	DBG_PRINT("Suspending at PC=%d, line=%p\n", PC, line);
	
	if (line != NULL) {
		switch_to_debugger("(STATUS Paused at 0x%x:%s)", line->addr, line->line);
	} else {
		DBG_PRINT("Error suspending @ PC=%d\n", PC);
	}
}

// Pastes tokens together e.g. {"abc", "def", "ghi"} -> "abcdefghi"
static char *merge_tokens(char **tokens, int start, int end) {
	int i, expr_len = 0;
	char *merged_tokens;
	
	for (i = start; i <= end; i++)
		expr_len += strlen(tokens[i]);
	
	merged_tokens = malloc(expr_len+1);
	strcpy(merged_tokens, tokens[start]);
	
	for (i = start+1; i <= end; i++)
		strcat(merged_tokens, tokens[i]);
	
	return merged_tokens;
}

/*
  Switches control from the simulator to debugger
  Accepts commands until the user enters run, step, or exit
*/
static void switch_to_debugger(char *title, ...) {
	int run = 0;
	char option;
	char *view_mem_options = "ar";
	char expr[256], command[1024];
	char full_title[4096]; // should be enough space
	va_list args;  
	Condition *cond;
	static int first_run = 1; // is set to 0 after the first time executing this function
	
	if (first_run) {
		write_to_dbg("Enter help to view a list of commands");
		write_to_dbg("Enter help <name of a command> for more info about a command");
		first_run = 0;
	}
	
	if (title != NULL) {
		va_start(args, title);
		vsprintf(full_title, title, args);
		va_end(args);
		set_window_title(dbg, full_title);
	}
    
	do {
		int i, found_one, num_args;
		long addr;
		char *args[32], *cmd_name;
		
		read_from_win(dbg, NULL, "%s", command);
		
		cmd_name = strtok(command, " ");
		
		if (cmd_name == NULL)
			continue;
		
		// break the command up into a sequence of tokens (where spaces occured)
		for (num_args = 0;
			 (args[num_args] = strtok(NULL, " ")) != NULL; num_args++)
			;
		
		if (strcmp(cmd_name, "r") == 0 || strcmp(cmd_name, "run") == 0) {
			run = 1;
		}
    
		/* examples:
		   step
		   step 8 */
		else if (strcmp(cmd_name, "s") == 0 || strcmp(cmd_name, "step") == 0) {
			int num_steps = 1;
      
			if (num_args >= 1 && valid_stol_str(args[0]) && stol(args[0]) > 0)
				num_steps = stol(args[0])+1;

			dbg_step = num_steps;
			run = 1;
		}
		
		else if (strcmp(cmd_name, "view") == 0) {
			if (num_args > 0) {
				if (strcmp(args[0], "s") == 0 || strcmp(args[0], "source") == 0)
					print_source();
				else if (strcmp(args[0], "l") == 0 || strcmp(args[0], "labels") == 0)
					print_labels();
				else if (strcmp(args[0], "w") == 0 || strcmp(args[0], "watches") == 0)
					print_condition_list("Conditions: ", watch_conditions);
				
				else if (strcmp(args[0], "r") == 0 || strcmp(args[0], "reg") == 0 ||
						 strcmp(args[0], "regs") == 0 || strcmp(args[0], "registers") == 0) {
					write_to_dbg("eax=0x%x, ecx=0x%x, edx=0x%x, ebx=0x%x, esp=0x%x, ebp=0x%x, esi=0x%x, edi=0x%x",
								 registers[0], registers[1], registers[2], registers[3], registers[4], registers[5],
								 registers[6], registers[7]);
					
					write_to_dbg("OF=%d, SF=%d, ZF=%d", flgs.OF, flgs.SF, flgs.ZF);
				}
				
				else if (strcmp(args[0], "breakpoints") == 0 ||
						 strcmp(args[0], "bp") == 0 || strcmp(args[0], "bps") == 0) {
					char *addr = args[1];
					
					if (addr == NULL) {
						// printing all breakpoints
						SourceLine *cur_line = source_lines;

						found_one = 0;
											
						while (cur_line != NULL) {
							if (cur_line->has_breakpoint) {
								write_to_dbg("Breakpoint at 0x%x", cur_line->addr);
								found_one = 1;
							}
							
							else if (cur_line->has_cond_breakpoint) {
								write_to_dbg("Conditional breakpoint(s) at 0x%x", cur_line->addr);
								found_one = 1;
							}
							
							cur_line = cur_line->next;
						}

						if (!found_one) {
							write_to_dbg("No breakpoints to list");
						}
					} else {
						// printing breakpoints at specified address
						if (valid_stol_str(addr)) {
							SourceLine *src_line = find_source_line(stol(addr));

							if (src_line != NULL) {
								found_one = 0;
								
								if (src_line->has_breakpoint) {
									write_to_dbg("Regular breakpoint at %s", addr);
									found_one = 1;
								}
								
								if (src_line->cond_bp_list != NULL) {
									print_condition_list("Conditional breakpoints: ", src_line->cond_bp_list);
									found_one = 1;
								}

								if (!found_one) {
									write_to_dbg("No breakpoints to list");
								}
							} else {
								write_to_dbg("No instruction at address %s", addr);
							}
						} else {
							write_to_dbg("Invalid address %s", addr);
						}
					}
				}
				
				else if (strcmp(args[0], "backtrace") == 0 || strcmp(args[0], "bt") == 0) {
					StackFrame *top = stack_frames;
					
					if (stack_frames == NULL) {
						write_to_dbg("No stack frames to print");
					}
					
					while (top != NULL) {
						write_to_dbg("0x%x:%s, esp at start=0x%x", top->addr, top->func_name, top->esp);
						top = top->next;
					}
				}
				
				else if (strcmp(args[0], "m") == 0 || strcmp(args[0], "mem") == 0 ||
						 strcmp(args[0], "memory") == 0) {
					char addr_low[32], addr_high[32];
					int low = 0, high = 0;
					
					read_from_win(dbg, "(a)ll, (r)ange", "%c", &option);
					
					while (strchr(view_mem_options, option) == NULL) {
						write_to_dbg("Invalid option");
						read_from_win(dbg, "(a)ll, (r)ange", "%c", &option);
					}
					
					if (option == 'r') {
						read_from_win(dbg, "Address low", "%s", addr_low);
						while (!valid_stol_str(addr_low) && strcmp(addr_low, "cancel") && strcmp(addr_low, "c"))
							read_from_win(dbg, "Invalid address low", "%s", addr_low);
						
						read_from_win(dbg, "Address high", "%s", addr_high);
						while (!valid_stol_str(addr_high) && strcmp(addr_high, "cancel") && strcmp(addr_high, "c"))
							read_from_win(dbg, "Invalid address high", "%s", addr_high);
						
						low = stol(addr_low);
						high = stol(addr_high);
					} else {
						low = 0;
						high = 4096;
						
						uint8 *p_high = &memory[4095];
						
						// we only print up to the point where the rest of memory is 0 
						while (*p_high == 0)
							p_high--;
						
						high = (int)(p_high - memory);
					}
					
					int count = 0;
					
					if (low <= high && low < 4096 && high < 4096) {
						char *formatted_mem_str = malloc(4096*6);
						memset(formatted_mem_str, 0, 4096*6);
						
						for (i = low; i <= high; i++) {
							if (count % 10 == 0) {
								if (count == 0)
									sprintf(formatted_mem_str, "0x%02x: %02x ", i, memory[i]);
								else {
									write_to_dbg("%s", formatted_mem_str);
									memset(formatted_mem_str, 0, 4096*6);
									sprintf(formatted_mem_str, "0x%02x: %02x ", i, memory[i]);
								}
							} else {
								sprintf(formatted_mem_str, "%s%02x ", formatted_mem_str, memory[i]);
							}
							
							count++;
						}

						write_to_dbg("%s", formatted_mem_str);
						write_to_dbg("-----------------------------------");
						free(formatted_mem_str);
					} else {
						write_to_dbg("Invalid address(s) supplied");
					}
				}
			} else {
				write_to_dbg("Missing arguments");
			}
		}
		
		// example: watch %ebx>8
		else if (strcmp(cmd_name, "watch") == 0) {
			int err;
			
			if (num_args > 0) {
				if (strcmp(args[num_args-1], "del")) {
					// adding a watch condition
					cond = malloc(sizeof(Condition)); // never freed when used
					
					char *expr_no_spaces = merge_tokens(args, 0, num_args-1);
					
					err = build_cond_by_expr(cond, expr_no_spaces);
					
					switch (err) {
					case SUCC:
						if (find_cond_by_expr(watch_conditions, expr_no_spaces) == NULL) {
							if (add_condition_list(&watch_conditions, cond))
								write_to_dbg("Added watch condition %s", expr_no_spaces);
							else
								write_to_dbg("Error adding watch condition");
						} else {
							write_to_dbg("Already watching for %s", expr_no_spaces);
							free(cond);
						}
						break;
					case MEM_ERR:
						write_to_dbg("Not enough memory");
						break;
					case INVALID_COND_EXPR:
						write_to_dbg("Invalid expression %s", expr_no_spaces);
						free(cond);
						break;
					}
					
					free(expr_no_spaces);
				}
				
				else if (strcmp(args[num_args-1], "del") == 0) {
					// deleting a watch condition
					cond = malloc(sizeof(Condition));
					
					char *expr_no_spaces = merge_tokens(args, 0, num_args-2);
					
					err = build_cond_by_expr(cond, expr_no_spaces);
					
					switch (err) {
					case SUCC:
						if (remove_condition_list(&watch_conditions, cond))
							write_to_dbg("Deleted watch condition %s", expr_no_spaces);
						else
							write_to_dbg("Could not find watch condition %s", expr_no_spaces);
						break;
					case MEM_ERR:
						write_to_dbg("Not enough memory");
						break;
					case INVALID_COND_EXPR:
						write_to_dbg("Invalid expression %s", expr_no_spaces);
						break;
					}
					
					free(cond);
					free(expr_no_spaces);
				}
				else {
					write_to_dbg("Invalid arguments");
				}
			} else {
				write_to_dbg("Missing arguments");
			}
		}
		
		/* examples:
		   bp 0x1f
		   bp myFunctionName
		   bp 0x2f if %ebx>8
		   bp 0x2f del */
		else if (strcmp(cmd_name, "bp") == 0 || strcmp(cmd_name, "break") == 0) {
			Label *func_label;
			SourceLine *src_line;
			int delete_bp = 0;
			
			if (num_args == 0) {
				write_to_dbg("Missing arguments");
				continue;
			}
			
			if (num_args > 2 && strcmp(args[1], "del") == 0) {
				write_to_dbg("Too many arguments: bp <address> del");
				continue;
			}
			
			if (num_args == 2)
				delete_bp = strcmp(args[1], "del") == 0;
			
			if (valid_stol_str(args[0])) // did we get an address?
				addr = stol(args[0]);
			
			else if ((func_label = find_label(args[0])) != NULL) // or a label?
				addr = func_label->addr;
			
			else {
				// if not address or label, its invalid
				write_to_dbg("Invalid input: %s", args[0]);
				continue;
			}
			
			src_line = find_source_line(addr);
			
			if (src_line == NULL) {
				write_to_dbg("No instruction at addr 0x%0x", addr);
				continue;
			}
			
			if (delete_bp) {
				// deleting breakpoint
				found_one = 0;
				
				if (src_line->has_breakpoint) {
					found_one = 1;
					read_from_win(dbg, "Breakpoint exists, delete it? (y)es or (n)o", "%c", &option);
					
					if (option == 'y') {
						src_line->has_breakpoint = 0;
						src_line->has_cond_breakpoint = 0;
						
						free_condition_list(src_line->cond_bp_list);
						src_line->cond_bp_list = NULL;
						
						write_to_dbg("Deleted breakpoint at 0x%x", addr);
					}
				}
				
				if (get_cond_list_size(src_line->cond_bp_list) > 0) {
					found_one = 1;
					read_from_win(dbg, "Conditional breakpoint(s) exists, delete one? (y)es or (n)o", "%c", &option);
					
					if (option == 'y') {
						print_condition_list("Conditions: ", src_line->cond_bp_list);
						read_from_win(dbg, "Enter a condition to delete", "%s", expr);
						remove_whitespaces(expr);
						cond = find_cond_by_expr(src_line->cond_bp_list, expr);
						
						if (remove_condition_list(&src_line->cond_bp_list, cond)) {
							write_to_dbg("Deleted breakpoint");
							
							if (get_cond_list_size(src_line->cond_bp_list) == 0)
								src_line->has_cond_breakpoint = 0;
						} else {
							write_to_dbg("Invalid condition");
						}
					}
				}
				
				if (!found_one) {
					write_to_dbg("No breakpoints at 0x%x to delete", addr);
				}
			}
			
			else if (num_args > 1 && strcmp(args[1], "if") == 0) {
				// adding a conditional breakpoint
				cond = malloc(sizeof(Condition));
				
				if (cond == NULL)
					return;
				
				/* the user should be able to enter "%edx>4" or "%edx > 4" or "%edx >4" etc..
				   so we ignore spaces by pasting all of the tokens together into full_expr */
				char *expr_no_spaces = merge_tokens(args, 2, num_args-1);
				
				if (build_cond_by_expr(cond, expr_no_spaces) != SUCC) {
					free(cond);
					free(expr_no_spaces);
					write_to_dbg("Invalid expression %s", expr_no_spaces);
					continue;
				}
				
				if (find_cond_by_expr(src_line->cond_bp_list, expr_no_spaces) == NULL) {
					if (add_condition_list(&src_line->cond_bp_list, cond)) {
						write_to_dbg("Added conditional breakpoint at 0x%x", src_line->addr);
						src_line->has_cond_breakpoint = 1;
					} else {
						write_to_dbg("Error adding breakpoint");
					}
				} else {
					write_to_dbg("Conditional breakpoint %s exists", expr_no_spaces);
					free(cond);
				}
				
				free(expr_no_spaces);
			}
			
			else {
				// adding an unconditional breakpoint
				if (!src_line->has_breakpoint) {
					src_line->has_breakpoint = 1;
					write_to_dbg("Added breakpoint at 0x%x", addr);
				} else {
					write_to_dbg("Already have a breakpoint at 0x%x", addr);
				}
			}
		}
		
		else if (strcmp(cmd_name, "pause") == 0) {
			if (num_args > 0) {
				if (gen_pause_file(args[0]))
					write_to_dbg("Wrote simulator state to %s", args[0]);
				else {
					write_to_dbg("Error writing simulator state to %s", args[0]);
					get_key_and_exit();
				}
				
				get_key_and_exit();
			} else {
				write_to_dbg("Missing arguments");
			}
		}
		
		else if (strcmp(cmd_name, "restore") == 0) {
			if (num_args > 0) {
				if (!restore_simulator_state(args[0])) {
					get_key_and_exit();
				}
			} else {
				write_to_dbg("Missing arguments");
			}
		}
		
		else if (strcmp(cmd_name, "makeyis") == 0) {
			if (num_args > 0) {
				if (gen_yo_file(args[0]))
					write_to_dbg("Wrote yo file to %s", args[0]);
				else
					write_to_dbg("Error writing to yo file at %s", args[0]);
			} else {
				write_to_dbg("Missing arguments");
			}
		}
		
		else if (strcmp(cmd_name, "exit") == 0) {
			get_key_and_exit();
		}
		
		else if (strcmp(cmd_name, "h") == 0 || strcmp(cmd_name, "help") == 0) {
			if (num_args == 0) {
				write_to_dbg("run, step, step <n>, exit");
				
				write_to_dbg("view <source/labels/registers/bps/bt/mem/watches>");
				
				write_to_dbg("watch <cond expr>, watch <cond expr> del");
				write_to_dbg("bp <addr/func name>, bp <addr/func name> del");
				write_to_dbg("bp <addr> if <cond expression>");
				write_to_dbg("bp <func name> if <cond expr>");
				
				write_to_dbg("pause <file name>, restore <file name>, write <file name>");
			} else {
				if (strcmp(args[0], "r") == 0 || strcmp(args[0], "run") == 0) {
					write_to_dbg("run - resumes execution of the program", args[0]);
				}
				
				else if (strcmp(args[0], "s") == 0 || strcmp(args[0], "step") == 0) {
					write_to_dbg("step - steps one instruction");
					write_to_dbg("step <n> - steps n instructions");
				}
				
				else if (strcmp(args[0], "watch") == 0) {
					write_to_dbg("watch <cond expr> - pauses program if cond expr holds");
					write_to_dbg("watch <cond expr> del - deletes watch condition");
					write_to_dbg("To learn more about conditional expressions, visit the READme");
				}
				
				else if (strcmp(args[0], "pause") == 0) {
					write_to_dbg("pause <file> - saves simulation state to file and exits");
				}
				
				else if (strcmp(args[0], "restore") == 0) {
					write_to_dbg("restore <file> - restores simulation state saved in file");
				}
				
				else if (strcmp(args[0], "makeyis") == 0) {
					write_to_dbg("makeyis <file> - generates a yis compatible yo file");
				}
				
				else if (strcmp(args[0], "bp") == 0) {
					write_to_dbg("bp <addr> - sets a breakpoint at an address");
					write_to_dbg("bp <func name> - sets a breakpoint at a function");
					write_to_dbg("bp <addr> if <cond expr> - sets a conditional breakpoint at addr");
					write_to_dbg("bp <func name> if <cond expr> - sets a conditional breakpoint at function"); 
					write_to_dbg("bp <addr/func> del - deletes breakpoint at address or function");
					write_to_dbg("To learn more about conditional expressions, visit the READme");
				}
				
				else if (strcmp(args[0], "exit") == 0) {
					write_to_dbg("exit - exits the simulator and debugger");
				}
				
				else if (strcmp(args[0], "view") == 0) {
					write_to_dbg("view source - view the source code of the program");
					write_to_dbg("view labels - view all labels and their addresses");
					write_to_dbg("view watches - view all watch conditions");
					write_to_dbg("view registers - view all register values");
					write_to_dbg("view bp - view all breakpoints");
					write_to_dbg("view bp <addr> - print breakpoint conditions on addr");
					write_to_dbg("view bt - view backtrace");
					write_to_dbg("view mem - view raw memory");
				}
				
				else {
					write_to_dbg("Invalid command name %s", args[0]);
				}
			}
		}
		
		else {
			write_to_dbg("Invalid command name %s", cmd_name);
		}
	} while (!run);
	
	set_window_title(dbg, NULL); // clear the debugger title now that we are done with the menu
}

// Prints a single SourceLine node to the window
static void print_source_line(SourceLine *line) {
	int i;
	char out[512];
	
	sprintf(out, "0x%03x: ", line->addr);
	
	if (!is_label_line(line->line) && strncmp(line->line, ".pos", 4) && strncmp(line->line, ".align", 6)) {
		int instr_size = get_instr_size(line->line, line->addr);
		
		for (i = line->addr; i < line->addr + instr_size; i++)
			sprintf(out, "%s%02x", out, memory[i]);
		
		for (i = 0; i < 13 - 2*instr_size; i++)
			strcat(out, " ");
		
		strcat(out, "| ");
		
		/* hopefully the user has not overwritten this executable code with
		   rmmovl. if so, this would no longer be the correct instruction string.
		   maybe make a rebuild_source_lines_from_mem function */
		strcat(out, line->line);
	} else {
		sprintf(out, "%s             | %s", out, line->line);
	}
	
	if (line->has_breakpoint)
		strcat(out, " (B)");
	else if (line->has_cond_breakpoint)
		strcat(out, " (C)");
	
	write_to_dbg("%s", out);
}

// Prints the source code to the y86 file
static void print_source() {
	char option;
	int key, start_addr, num_printed = 0;
	SourceLine *cur = source_lines;
	
	read_from_win(dbg, "Print from (t)op, (c)urrent instruction or an (a)ddress", "%c", &option);
	
	if (option == 't')
		start_addr = 0;
	else if (option == 'c')
		start_addr = sim_get_pc();
	else {
		char addr[32];
		
		read_from_win(dbg, "Enter the address to start at", "%s", addr);
		while (!valid_stol_str(addr) && strcmp(addr, "cancel") && strcmp(addr, "c"))
			read_from_win(dbg, "Invalid address", "%s", addr);
		
		start_addr = stol(addr);
	}
	
	// dont print more source than will fit into console
	while (num_printed < num_dbg_lines-5 && cur != NULL) {
		if (cur->addr >= start_addr) {
			print_source_line(cur);
			num_printed++;
		}
		
		cur = cur->next;
	}
	
	// print the rest
	while (cur != NULL) {
		mvwprintw(dbg, num_dbg_lines-2, 1, "Press p to print one line of source, d for done");
		key = wgetch(dbg);
		
		if (key == 'p') {
			print_source_line(cur);
			cur = cur->next;
		} else {
			break;
		}
	}
}

// Prints all labels and their addresses
static void print_labels() {
	int key_in;
	int num_printed = 0;
	
	if (num_labels == 0) {
		write_to_dbg("No labels to print");
		return;
	}
	
	// dont print more than will fit into console
	while (num_printed < num_dbg_lines-5 && num_printed < num_labels) {
		write_to_dbg("Label- 0x%x:%s", labels[num_printed]->addr, labels[num_printed]->name);
		num_printed++;
	}
	
	while (num_printed < num_labels) {
		mvwprintw(dbg, num_dbg_lines-2, 1, "Press p to print another label, d for done");
		key_in = wgetch(dbg);
		
		if (key_in == 'p') {
			write_to_dbg("Label- 0x%x:%s", labels[num_printed]->addr, labels[num_printed]->name);
			num_printed++;
		} else {
			break;
		}
	}
}

// Prints a single Condition
static void print_condition(Condition *cond) {
	char expr[256];
	
	strcpy(expr, cond->x);
	
	switch (cond->op) {
	case OP_L:
		strcat(expr, "<");
		break;
	case OP_G:
		strcat(expr, ">");
		break;
	case OP_EQ:
		strcat(expr, "=");
		break;
	case OP_GEQ:
		strcat(expr, ">=");
		break;
	case OP_LEQ:
		strcat(expr, "<=");
		break;
	case OP_NEQ:
		strcat(expr, "!=");
		break;
	}
	
	strcat(expr, cond->y);
	write_to_dbg("\t%s", expr);
}

// Prints a condition linked list
void print_condition_list(char *list_title, ConditionList *list) {
	int key, num_printed = 0;
	ConditionList *cur = list;
	
	if (list == NULL) {
		write_to_dbg("No conditions to print");
		return;
	}
	
	if (list_title != NULL)
		write_to_dbg("%s", list_title);
	
	num_printed = 1;
	
	// dont print more source than will fit into console
	while (num_printed < num_dbg_lines-5 && cur != NULL) {
		print_condition(cur->con);
		num_printed++;
		cur = cur->next;
	}
	
	// print the rest
	while (cur != NULL) {
		mvwprintw(dbg, num_dbg_lines-2, 1, "Press p to print one more watch condition, d for done");
		key = wgetch(dbg);
		
		if (key == 'p') {
			print_condition(cur->con);
			cur = cur->next;
		} else {
			break;
		}
	}
}
