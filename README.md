To compile y86sim type make in the root directory. If you get the error "curses.h: No such file or directory" then you need to install the ncurses library on your machine. This can be done using apt-get: "apt-get install libncurses5-dev libncursesw5-dev" or yum: "yum install ncurses-devel ncurses".

To run a y86 program, pass y86sim the name of the y86 source file as a command line argument, for example: ./y86sim myfile.y86

The simulator will start off paused with the debugger waiting to accept a command.

In the following list of commands we use the notation \<addr\> to stand for an address, \<func name\> to stand for a function name, \<file name\> to stand for a file name, and \<cond expr\> to stand for a conditional expression. These place holders are explained in more detail below.

Debugger commands:
 * run -- Resumes execution of the program
 * step -- Executes 1 instruction of the program and returns to the debugger
 * step \<n\> -- Executes n instructions of the program and returns to the debugger
 * bp \<addr\> -- Sets a breakpoint at \<addr\>. Prior to executing the instruction at \<addr\>, the simulator will pause and the debugger will activate.
 * bp \<func name\> -- Same as above but takes a function name
 * bp \<addr\> if \<cond expr\> -- Sets a conditional breakpoint at \<addr\>. Whenever \<cond expr\> holds prior to executing the instruction at \<addr\>, the simulator will pause and the debugger will activate.
 * bp \<func name\> if \<cond expr\> -- Same as above but takes a function name
 * bp \<addr\> del -- Deletes breakpoint(s) at \<addr\>. You will be prompted for options to delete all breakpoints (conditional and unconditional) or just a single conditional breakpoint.
 * bp \<func name\> del -- Same as above but takes a function name
 * watch \<cond expr\> -- Adds a watch condition for \<cond expr\>. Whenever \<cond expr\> holds prior to executing any instruction, the simulator will pause and the debugger will activate.
 * watch \<cond expr\> del -- Deletes the watch condition for \<cond expr\>. 
 * view source -- Prints the source of the y86 file. You will be asked whether you want to print from the top, the current instruction, or a specified address.
 * view labels -- Prints all labels in the source file
 * view registers -- Prints all register values and flag values
 * view bps -- Prints all breakpoints (conditional and unconditional)
 * view bps \<addr\> -- Prints all breakpoints at \<addr\>
 * view watches -- Prints all watch conditions
 * view bt -- Prints a backtrace of active function calls
 * view mem -- Prints raw memory. You will be prompted for an option to print all of memory or a range of memory.
 * pause \<file name\> -- Saves the state of the simulator and debugger to a file (including all breakpoints, watch conditions, etc..) to be restored at a later time
 * restore \<file name\> -- Restores the state of the simulator and debugger from a file at the same point of execution
 * makeyis \<file name\> -- Generates a yis compatible .yo file
 * exit -- Exits the simulator

\<addr\> is an address and may be in decimal (e.g. 53) or hexadecimal (e.g. 0x35)  
\<func name\> is the name of a function (i.e. the name of it's label)  
\<file name\> is the name of a file to save to/read from  
\<cond expr\> is a conditional expression and takes the format: \<value descriptor\>\<relational operator\>\<value descriptor\>.  

A value descriptor is a string which can describe three types of values:
 1. A constant value, which is optionally preceded by $ (e.g. 237 or 0xdeadc0de or $237 or $0xa3)
 2. A register value, which must be preceded by % (e.g. %edx or %ebx)
 3. A value in memory, which takes the form "[base address, number of bytes to read]"  
         where number of bytes to read is 1, 2 or 4  
	 e.g. [0,2] evaluates to the 16 bit integer stored in the first 2 bytes of memory  
	      [0x33,4] evalutes to the 32 bit integer starting at address 0x33  
          and [4,0x1] evaluates to the byte located at address 4  

A relational operator is one of the following: \<, \>, =, \>=, \<=, != (notice we use = and not ==).

Here are some examples of conditional breakpoints and watch condition commands:
 - bp 0x1a if %edx\>0
 - bp 22 if 9=9 (Equivilant to an unconditional breakpoint at address 22)
 - bp 28 if [23,1]=0xff
 - watch %ecx!=[80,4]
 - watch $0x3a=$0x3a (Has the effect of returning to the debugger prior to executing every instruction)
 - watch %esp=%ebp

WARNING: y86sim does not support labels appearing on the same line as an instruction or directive. This is inconsistent with yas/yis, which allows source lines such as "myFunc: pushl %ebp" or "myData: .long 3". To overcome this limitation, simply place all labels on their own line, for example:  
myFunc:  
  pushl %ebp  

and

myData:  
  .long 3  
