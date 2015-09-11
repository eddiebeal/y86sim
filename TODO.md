
TODO:  
* Support labels and instructions on same line
* Should be able to support .long \<label name\>, where \<label name\> will resolve to the address of the label
* Add support for jmp $constant, call $constant (only jmp \<label name\> and call \<label name\> is supported)
* Add support for conditional moves (cmovle, cmovl, ...) -- more info on slide 9 of http://www.ugrad.cs.ubc.ca/~cs313/2011S/slides/Y86-Sequential.
* If a callback fails, it should include in the print message the address at which it failed (currently only prints the instruction) 
* When paused in debugger, make it say why (for a breakpoint? returning from step? watch condition (if so, which one?))
* Make a third smaller window that shows the previous few instructions and the next few instructions
* View stack command (or window with stack)
* Add a step over function calls command to debugger
* When restoring/pausing to a file fails, we exit(0), since we were in the middle of modifying the simulator state. Save original state and don't silently fail.
* More #define'd constants, we have a lot of magic numbers in the code
* Should also change functions to use the return codes #define'd in common.h (e.g. SUCC, MEM_ERR), make a FAIL
* I was lax on freeing allocated memory since the simulator isn't much of a memory hog, so there are certainly some memory leaks to be addressed
* In write_to_dbg detect if the string is too big to fit in the console (in x dimension, break up into multiple messages?)
* The parser needs to recognize invalid source files and write a message instead of silent failure or even worse a buggy run
* A lot of the DBG_PRINT's that show error messages should actually be write_to_dbg's to notify user of errors
* Check in assembler if we go past 4096 bytes
* Make sure the parser can read files using \n and \r\n as newlines
