// common.c - Contains general utility functions used across the project
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include "common.h"

FILE *f_dbg = NULL;

// Opens the file used by DBG_PRINT for debugging purposes
void init_dbg_print() {
#ifdef DEBUG
	f_dbg = fopen("dbg.txt", "w+");
#endif
}

// Closes the file used by DBG_PRINT
void destroy_dbg_print() {
#ifdef DEBUG
	fclose(f_dbg);
	f_dbg = NULL;
#endif
}

/*
  Converts a hex value as an ASCII character into the integer equivilant
  Returns 16 if an invalid character is supplied, or an integer
  in the range 0-15 otherwise
*/
uint32 hex_char_to_val(char c) { // uint8?
	if (c >= '0' && c <= '9')
		return c - '0';
  
	else {
		if (c == 'a' || c == 'A')
			return 10;
		if (c == 'b' || c == 'B')
			return 11;
		if (c == 'c' || c == 'C')
			return 12;
		if (c == 'd' || c == 'D')
			return 13;
		if (c == 'e' || c == 'E')
			return 14;
		if (c == 'f' || c == 'F')
			return 15;
	}
	
	return 16; // error
}

// Converts a hexidecimal string to an unsigned integer, returns -1 on error
uint32 hexstr2long(char *str) {
	uint32 val = 0;
	char *start = str, *end = str;
	int idx = 0;
	
	if (str == NULL) {
		DBG_PRINT("Error: NULL str supplied\n");
		return -1;
	}

	while (*end != '\0')
		end++;
	end--;

	while (end != start-1) {
		uint32 multiplier = hex_char_to_val(*end);
		
		if (multiplier != 16) {
			val += multiplier*pow(16, idx++);
		} else {
			DBG_PRINT("Error: Invalid hexadecimal digit supplied (%c).\n", *end);
			return -1;
		}
		
		end--;
	}

	return val;
}

// Verifies that str is a valid string to pass to stol
int valid_stol_str(char *str) {
	int i;

	if (str == NULL || *str == '\0')
		return 0;
	
	if (*str == '-')
		str++;
  
	if (str[0] == '0' && str[1] == 'x') {
		if (str[2] == 0) // need atleast one hexadecimal digit to be valid
			return 0;
		
		for (i = 2; i < strlen(str); i++)
			if (hex_char_to_val(str[i]) == 16)
				return 0;
    
		return 1;
	} else {
		for (i = 0; i < strlen(str); i++) 
			if (!isdigit(str[i]))
				return 0;
    
		return 1;
	}
}

/*
  String to long
  If str begins with 0x then it is handled as a hexidecimal value
  Otherwise it is handled as base 10
*/
long int stol(char *str) {
	int mult = 1;
  
	if (!valid_stol_str(str))
		return -1;
	
	if (*str == '-') {
		mult = -1;
		str++;
	}
  
	if (str[0] == '0' && str[1] == 'x')
		return mult * hexstr2long(str+2);
	else
		return mult * atol(str);
}

// Round "num" up to closest multiple of "to"
int round_up_to_nearest(int num, int to) {
	if (to == 0)
		return num;
	
	int remainder = num % to;
	
	if (remainder == 0)
		return num;
	else
		return (to-remainder)+num;
}

// Returns 1 if c is a whitespace and 0 if not
int is_whitespace(char c) {
	return (c >= 9 && c <= 13) || (c == ' ');
}

// Returns 1 if c is an alphanumeric character and 0 if not
int is_alphanumeric(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

/*
  Return a pointer to the first instance of an alphanumeric character in buf
  or NULL if none such character exists
*/
char *get_first_alphanumeric(char *buf) {
	int i;

	if (buf == NULL)
		return NULL;
  
	for (i = 0; i < strlen(buf); i++)
		if (is_alphanumeric(buf[i]))
			return &buf[i];
	
	return NULL;
}

// Returns 1 if str ends with the character c and 0 if not
int str_ends_with(char *str, char c) {
	int len;

	if (str == NULL)
		return 0;
	
	len = strlen(str);
	if (len == 0)
		return 0;

	return str[len-1] == c;
}

// Returns the number of times the character c appears in str
int char_count(char *str, char c) {
	int count = 0;

	if (str == NULL)
		return 0;
  
	while (*str != '\0') {
		if (*str == c)
			count++;
		str++;
	}

	return count;
}

// Removes any whitespaces from str
void remove_whitespaces(char *str) {
	char *no_spaces;
	int i, idx = 0;

	if (str == NULL)
		return;

	no_spaces = strdup(str);
	
	if (no_spaces == NULL)
		return;

	memset(no_spaces, 0, strlen(str) + 1);
  
	for (i = 0; i < strlen(str); i++)
		if (!is_whitespace(str[i]))
			no_spaces[idx++] = str[i];

	strcpy(str, no_spaces);
	free(no_spaces);
}
