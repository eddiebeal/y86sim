// console.c - Contains code to manage the simulator/debugger console windows
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ncurses.h>
#include "console.h"
#include "common.h"

// CONFIGURABLE //
float dbg_win_frac = .5; // fraction of the console for the debugger

// DO NOT TOUCH //
WINDOW *sim = NULL, *dbg = NULL; // window handles
int num_dbg_lines = 0; // number of lines used by the debugger (calculated by init_console)
int num_sim_lines = 0;
int line_width = 0; // width of each line, set by init_console
int num_lines = 0; // number of lines total in the console, set by init_console
char **sim_lines = NULL; // holds the lines as they are shown in the console, with the exception that a new line on line x is stored as simLine[x] = "" (no \n stored)
char **dbg_lines = NULL;
int cur_sim_line = 3, next_dbg_line = 3; // next line number to print to
int sim_window_overflow = 0; // i.e. have we written past the last line and moved all lines of text up one? (need to store for pausing&restoring)

/*
  We need to save the current titles of the windows, since write_to_win clears
  the entire window when a new line is added that goes past the bottom, requiring us
  to redraw the title
*/
char sim_title[512], dbg_title[512];

/*
  Initializes the console, setting up the screen and the two windows with the
    specified number of lines for the simulator window and the debugger window
    as declared in num_sim_lines and num_dbg_lines
*/
void init_console() {
	static int initialized = 0;
	
	if (!initialized) {
		int i, x, y;
		
		initscr();
		getmaxyx(stdscr, y, x);
		
		if (y < 19) {
			printf("Console vertical size too small\n");
			exit(0);
		}
		
		num_dbg_lines = (int)(dbg_win_frac * y);
		
		if (num_dbg_lines < 13) // we need atleast 13 lines to show full help menu
			num_dbg_lines = 13;
		
		num_sim_lines = y - num_dbg_lines; // sim window gets whatever left over
		
		line_width = x;
		num_lines = y;
		
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
		
		sim = newwin(num_sim_lines, x, 0, 0);
		dbg = newwin(num_dbg_lines, x, y - num_dbg_lines, 0);
		
		setup_window(sim, "Simulator");
		setup_window(dbg, "Debugger");
		
		refresh_console();
		
		initialized = 1;
	}
}

// Clears the window, including its name, title, and output lines
void clear_window(WINDOW *win) {
	int i;
	
	if (win != sim && win != dbg)
		return;
	
	werase(win);
	
	if (win == sim) {
		cur_sim_line = 3;
		for (i = 0; i < num_sim_lines; i++)
			memset(sim_lines[i], 0, line_width+1);
	} else {
		next_dbg_line = 3; 
		for (i = 0; i < num_dbg_lines; i++)
			memset(dbg_lines[i], 0, line_width+1);
	}
}

// Clears a single line of a window
static void clear_line_by_num(WINDOW *win, int line_num) {
	if (win != sim && win != dbg)
		return;
	
	wmove(win, line_num, 0);
	wclrtoeol(win);
	box(win, 0, 0); // need to redraw box around whole window, since we erased the part on input line with the wclrtoeol call
	wrefresh(win);
}

/*
  Sets up the window for either the debugger or simulator
  The window name is displayed on the second line, with the window border on the first line
  A horizontal line is added to the third line to separate the window name
    from the actual part of the window used for I/O
*/
void setup_window(WINDOW *win, char *name) {
	if (win != sim && win != dbg)
		return;
	
	box(win, 0, 0); // add border around entire window
	mvwprintw(win, 1, 1, "%s", name);
	mvwhline(win, 2, 1, '-', line_width-2);
}

/*
  Sets the "title" of the window (i.e. the text right after the name of the window)
  If title is NULL then the title area will be blank
*/
void set_window_title(WINDOW *win, char *title) {
	if (win != sim && win != dbg)
	  return;
  
	if (win == sim) {
		clear_line_by_num(sim, 1);
		mvwprintw(sim, 1, 1, "Simulator");
		
		if (title != NULL) {
			mvwprintw(sim, 1, 11, "%s", title);
			strcpy(sim_title, title);
		}
		
		wrefresh(sim);
	} else {
		clear_line_by_num(dbg, 1);
		mvwprintw(dbg, 1, 1, "Debugger");
		
		if (title != NULL) {
			mvwprintw(dbg, 1, 10, "%s", title);
			strcpy(dbg_title, title);
		}
		
		wrefresh(dbg);
	}
}

// Prints a single line, stored at sim_lines[line_num], to the simulator window
static void print_single_sim_line(int line_num) {
	if (str_ends_with(sim_lines[line_num], '\n')) {
		DBG_PRINT("Printing one sim line @ %d, ends with new line\n", line_num);
		int len = strlen(sim_lines[line_num]);
		sim_lines[line_num][len-1] = '\0';
		mvwprintw(sim, line_num, 1, "%s", sim_lines[line_num]);
		sim_lines[line_num][len-1] = '\n';
	} else {
		DBG_PRINT("Printing one sim line @ %d, does not end with new line\n", line_num);
		mvwprintw(sim, line_num, 1, "%s", sim_lines[line_num]);
	}
}

/*
  Redraws the window, called whenever sim_lines/dbg_lines is modified directly
   and we need to show the new text
*/
void redraw_window(WINDOW *win) {
	int i, lim;
	
	if (win == sim) {
		werase(sim);
		setup_window(sim, "Simulator");
		
		if (sim_title != NULL)
			set_window_title(sim, sim_title);

		lim = sim_window_overflow ? cur_sim_line-1 : cur_sim_line;
		DBG_PRINT("sim_window_overflow=%d, lim=%d\n", sim_window_overflow, lim);
		
		for (i = 3; i <= lim; i++) {
			clear_line_by_num(sim, i);
			print_single_sim_line(i);
		}
		
		wrefresh(sim);
	} else {
		werase(dbg);
		setup_window(dbg, "Debugger");
		
		if (dbg_title != NULL)
			set_window_title(dbg, dbg_title);
		
		for (i = 3; i < next_dbg_line; i++) {
			clear_line_by_num(dbg, i);
			mvwprintw(dbg, i, 1, "%s", dbg_lines[i]);
		}
		
		wrefresh(dbg);
	}
}

/*
  Writes to the simulator. To emulate an actual terminal window we need
   to replicate the scroll feature, where if we run out of room (lines) in the window,
   then we make room for the new line at the bottom by moving each line of text up one line.
*/
void write_to_sim(char *str, ...) {
	char formatted[4096]; // might not be enough space, rename to out
	va_list args;
	int i, new_line;
	
	va_start(args, str);
	vsprintf(formatted, str, args);
	va_end(args);
	
	DBG_PRINT("Attempting to write %s to simulator\n", formatted);
	
	new_line = strcmp(formatted, "\n") == 0;
	
	/*
	  if we have enough room in the window
        print the text
	  otherwise
        move each line of text up one line, making room for the new line at the bottom and print it there
	*/
  
	if (cur_sim_line < num_sim_lines - 2) { 
		if (strlen(sim_lines[cur_sim_line]) + strlen(formatted) > line_width - 2) {
			cur_sim_line++;
			strcat(sim_lines[cur_sim_line], formatted);
		} else {
			strcat(sim_lines[cur_sim_line], formatted);
			if (new_line) {
				cur_sim_line++;
			}
		}
		
		if (cur_sim_line == num_sim_lines - 2)
			sim_window_overflow = 1;
		
		// notice if the string is a new line, we dont print it right away, but wait to shift the lines down next time we print
		if (!new_line) {
			print_single_sim_line(cur_sim_line);
		}
	} else {
		if (str_ends_with(sim_lines[cur_sim_line-1], '\n') || strlen(sim_lines[cur_sim_line-1]) + strlen(formatted) > line_width - 2) {
			// need to move each line of text up one line, making room for this line at cur_sim_line-1
			for (i = 3; i < cur_sim_line-1; i++) {
				memset(sim_lines[i], 0, line_width+1);
				strcpy(sim_lines[i], sim_lines[i+1]);
			}
			
			if (!new_line)
				strcpy(sim_lines[cur_sim_line-1], formatted);
			else
				memset(sim_lines[cur_sim_line-1], 0, line_width+1);
			
			redraw_window(sim);
		}
		
		else {
			strcat(sim_lines[cur_sim_line-1], formatted);
			clear_line_by_num(sim, cur_sim_line-1);
			print_single_sim_line(cur_sim_line-1);
		}
	}
	
	wrefresh(sim);
}

// Writes a single line of text to the debugger
void write_to_dbg(char *str, ...) {
	int i;
	char formatted[4096]; // might not be enough space
	va_list args;  
  
	va_start(args, str);
	vsprintf(formatted, str, args);
	va_end(args);
	
	DBG_PRINT("Attempting to write %s to the debugger\n", formatted);
	
	if (next_dbg_line < num_dbg_lines - 2) { // first 3 lines are taken by title+border lines, last line is reserved for user input
		// we have enough room in the window, so just print the text
		strcpy(dbg_lines[next_dbg_line], formatted);
		mvwprintw(dbg, next_dbg_line, 1, "%s", formatted);
		next_dbg_line++;
	} else {
		// need to move each line of text up a line
		for (i = 3; i < next_dbg_line-1; i++)
			strcpy(dbg_lines[i], dbg_lines[i+1]);
		
		strcpy(dbg_lines[next_dbg_line-1], formatted);
		redraw_window(dbg);
	}
  
	wrefresh(dbg);
}

/*
  Reads formatted input into buf from a window.
  The input is taken from the second to last line of the window (the last line is used by the border)
  If input_title is NULL, then only a "> " is used to prompt the user
  Otherwise, say if input_title is Enter a string, then "Enter a string > " will be displayed
*/
void read_from_win(WINDOW *win, char *input_title, char *format, void *buf) {
	if (win != dbg && win != sim)
		return;
  
	int num_lines = (win == dbg) ? num_dbg_lines : num_sim_lines;
  
	clear_line_by_num(win, num_lines-2);

	if (input_title != NULL) {
		char *input_prompt = malloc(strlen(input_title)+8);
		sprintf(input_prompt, "%s > ", input_title);
		mvwprintw(win, num_lines-2, 1, "%s", input_prompt);
		
		if (strcmp(format, "%s") == 0)
			mvwgetstr(win, num_lines-2, strlen(input_prompt) + 1, buf); // mvwscanw does not handle spaces in single calls
		else
			mvwscanw(win, num_lines-2, strlen(input_prompt) + 1, format, buf);
		
		free(input_prompt);
	} else {
		mvwprintw(win, num_lines-2, 1, "> ");
		
		if (strcmp(format, "%s") == 0)
			mvwgetstr(win, num_lines-2, 3, buf);
		else
			mvwscanw(win, num_lines-2, 3, format, buf);
	}
	
	wrefresh(win);
	clear_line_by_num(win, num_lines-2);
}

// Refreshes the simulator and debugger windows
void refresh_console() {
	wrefresh(sim);
	wrefresh(dbg);
}

// Waits for the user to press a key and exits the program
void get_key_and_exit() {
	clear_line_by_num(sim, num_sim_lines-2);
	
	mvwprintw(sim, num_sim_lines-2, 1, "Press any key to exit > ");
	wrefresh(sim);
	wgetch(sim);
	
	destroy_console();
	destroy_dbg_print();
	exit(0);
}

// Frees the sim_lines and dbg_lines arrays
void free_dbg_and_sim_lines() {
	int i;

	if (dbg_lines != NULL) {
		for (i = 0; i < num_dbg_lines; i++)
			if (dbg_lines[i] != NULL)
				free(dbg_lines[i]);
		
		free(dbg_lines);
		dbg_lines = NULL;
	}

	if (sim_lines != NULL) {
		for (i = 0; i < num_sim_lines; i++)
			if (sim_lines[i] != NULL)
				free(sim_lines[i]);
		
		free(sim_lines);
		sim_lines = NULL;
	}
}

// Destroys the console by freeing all resources and closing the window
void destroy_console() {  
	free_dbg_and_sim_lines();  
	delwin(sim);
	delwin(dbg);
	endwin();
}
