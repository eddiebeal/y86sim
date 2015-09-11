# :( sad Makefile that wants more dependencies

y86sim: assembler.c assembler.h common.c common.h console.c console.h simulator.c simulator.h debugger.c debugger.h parser.c parser.h pause.c pause.h condition.c condition.h main.c
	gcc -o y86sim main.c simulator.c console.c debugger.c common.c assembler.c parser.c pause.c condition.c -lm -lncurses -g -Wall
