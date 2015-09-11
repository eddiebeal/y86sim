#ifndef CONSOLE_H
#define CONSOLE_H
#include <curses.h>

extern WINDOW *sim, *dbg;
extern float dbg_win_frac;
extern int cur_sim_line, next_dbg_line, sim_window_overflow;
extern int num_sim_lines, num_dbg_lines, line_width, num_lines;
extern char **dbg_lines, **sim_lines;
extern char sim_title[], dbg_title[];

void init_console();
void resize_windows();
void setup_window(WINDOW*, char*);
void clear_window(WINDOW *win);
void redraw_window(WINDOW *win);
void set_window_title(WINDOW *win, char *title);
void write_to_sim(char *str, ...);
void write_to_dbg(char *str, ...);
void read_from_win(WINDOW *win, char *input_title, char *format, void *buf);
void refresh_console();
void get_key_and_exit();
void free_dbg_and_sim_lines();
void destroy_console();

#endif
