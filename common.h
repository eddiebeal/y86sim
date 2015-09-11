#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdint.h>

// common to all functions
#define SUCC 0
// used by: <...>
#define MEM_ERR 1
#define INVALID_COND_EXPR 2
// used by get_val_desc_val
#define INVALID_VAL_DESC 1
// used by gen_byte_code
#define INVALID_FILE 1
#define PARSE_ERROR 2

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

//#define DEBUG

extern FILE *f_dbg;

#ifdef DEBUG
// DBG_PRINT adapted from http://stackoverflow.com/questions/1941307/c-debug-print-macros
#define DBG_PRINT(fmt, args...) fprintf(f_dbg, "DEBUG: %s:%d:%s(): " fmt, \
					__FILE__, __LINE__, __func__, ##args); \
                                fflush(f_dbg)
#else
#define DBG_PRINT(fmt, args...) do {} while (0)
#endif

char *get_first_alphanumeric(char *buf);
int is_alphanumeric(char c);
int is_whitespace(char c);
int round_up_to_nearest(int num, int to);
uint32 hex_char_to_val(char c);
uint32 hexstr2long(char *str);
int valid_stol_str(char *str);
long int stol(char *str);
int str_ends_with(char *str, char c);
int char_count(char *str, char c);
void remove_whitespaces(char *str);
void init_dbg_print();
void destroy_dbg_print();

#endif
