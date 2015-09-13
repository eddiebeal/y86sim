Common

Note: DBG_PRINT appears in most source files and has nothing to do with the actual y86 debugger. It is simply a macro that writes to a log file (dbg.txt) when DEBUG is #define'd for the debugging purposes of the developer. It attaches to the output message the source file, line number and function name of where DBG_PRINT was called from. The log file is opened by init_dbg_print and closed by destroy_dbg_print.


-----------------------------------------------------------------

Console

We use the ncurses library to split the entire console window into two smaller console windows. The top window is used by the simulator and the bottom window is used by the debugger. The debugger gets a certain percentage (dbg_win_frac*100) of the entire console (measured in lines), while the simulator gets the rest.

Both windows have the same general format. The first and last lines are used by the border. The second line contains the labels "Simulator" or "Debugger" and optionally a title to inform the user of the state of that window. For example, when paused waiting for the user to input an integer, the simulator title is (STATUS: Waiting for integer input - rdint). This title is set by set_window_title.

The third line is filled with '-' characters. This is done for clarity, to separate the label and title information on the second line from the console part of the window starting on the 4th line, which is used for output.

The second to last line is reserved for user input.

write_to_dbg and write_to_sim are the functions which print text to the debugger and simulator windows. They start printing at line 4, where all output goes.

In order to emulate an actual console window, we need to replicate the scroll feature of the console, where if we run out of room (lines) in the window, then we make room for the new line at the bottom by moving each line of text up by a single line.

To achieve this, we store all of the lines currently written to the two windows in an array of strings (sim_lines and dbg_lines).

We create two separate functions write_to_dbg and write_to_sim, instead of a single function with a window handle as a parameter because writing to the debugger window is slightly different than writing to the simulator window. Since y86 only has a wrch and wrint instruction, we are only outputting one character (or one integer) at a time to the simulator window. Thus, new lines are inserted manually by wrch'ing 0x0a (0x0a='\n'). On the other hand, when writing to the debugger window, we generally print an entire string and new lines are inserted implicitly at every call.


-----------------------------------------------------------------

Assembler

The core of the assembler consists of codegen functions which perform the instruction encoding for the set of y86 instructions. We group codegen functions together based on the general format of the operands, as opposed to having a codegen function for each instruction (\*). For example, rdint, rdch, wrint, wrch, pushl, and popl are grouped together in the function reg_num_codegen because they all have a single 8 bit operand representing a register number. Codegen functions have two arguments: 1) char \*cmd is the name of the instruction, e.g. "pushl" 2) char \*\*args stores the arguments to the instruction, e.g. if we have pushl %ebx, then args[0]="%ebx".

gen_bytecode is the function which builds the program memory. It does this by reading the source file line by line, passing each line to the parse_line function of the parser, which in turn calls the appropriate codegen function.  

(\*) This is true with the exception of irmovl_callback, long_callback, pos_callback, and align_callback.


-----------------------------------------------------------------

Simulator

The bulk of the simulator consists of callback functions for each y86 instruction (e.g. irmovl, jmp, addl). The callback functions simulate the execution of the instruction on a processor. Each callback function has a comment explaining the format of the instruction encoding for that instruction, which consists of the opcode (first byte) and any operands annotated with their size (BYTE/UINT16/UINT32) and what they represent.

The simulator also contains the exec_bytecode function which consists of a loop that reads the opcode of the next instruction to execute (located at the program counter – PC, aka the instruction pointer), looks up the callback function corresponding to the instruction with that opcode, and executes it.


-----------------------------------------------------------------

Debugger

To support conditional breakpoints and watch conditions, we need some way of representing a condition.

This is done in condition.c/condition.h which defines a struct Condition, consisting of three elements: x and y, which are value descriptors (described below), and an enum op, which represents a relational operator (e.g. less than, greater than or equal to, etc..). 

A value descriptor is a string which can describe three types of values:
 1. A constant value, which is optionally preceded by $ (e.g. 237 or 0xdeadc0de or $237 or $0xa3)
 2. A register value, which must be preceded by % (e.g. %edx or %ebx)
 3. A value in memory, which takes the form "[base address, number of bytes to read]"  
         where number of bytes to read is 1, 2 or 4  
	 e.g. [0,2] evaluates to the 16 bit integer stored in the first 2 bytes of memory  
	      [0x33,4] evaluates to the 32 bit integer starting at address 0x33  
          and [4,0x1] evaluates to the byte located at address 4  

We allow multiple watch conditions to be watched for and multiple conditional breakpoints to be placed on a single address. Thus, we store a collection of conditions in a struct ConditionList, which is a linked list holding struct Conditions.


-----------------------------------------------------------------

Parser

struct SourceLine defines a linked list containing a node for each line of the y86 source. It contains a 16 bit integer holding the address the line occupies, two boolean variables has_breakpoint and has_cond_breakpoint which are self explanatory, and a ConditionList cond_bp_list which stores the conditions for each conditional breakpoint. The global struct SourceLine source_lines linked list is built in parse_labels (it just so happens to be the most convenient function to build it in).


-----------------------------------------------------------------

Pausing & Restoring

In order to pause the simulator to be restored at a later point, we save the state of the simulator, debugger, and console to a file.

The file is generated by gen_pause_file in pause.c and is structured as follows: 
 registers, PC, flags, memory, watch conditions, source lines, [9 global variables from debugger.c],
 num_dbg_lines, num_sim_lines, sim_lines array (element by element), dbg_lines array (element by element)

All of these variables are written to the file by a call to fwrite with the exception of watch_conditions and source_lines, since they are stored in linked lists.

In order to store the linked lists, we first write the size of the linked list as a 16 bit integer, and then recursively write each node in the linked list, starting at the head.

Strings in the linked list are written by first writing the length of the string (including the null byte), and then writing the string itself (including the null byte). The strings in the linked list nodes are written this way because they are dynamically allocated and do not have a fixed size.

Restoring the state of a simulator instance saved to a file is accomplished by restore_simulator_state. This function reads the variables back in from the file, updates the debugger and console windows, and resumes execution of the program by calling exec_bytecode.
