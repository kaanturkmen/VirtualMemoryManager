all: compile run 

compile:
	gcc -o part2 part2.c

run:
	./part2 BACKING_STORE.bin addresses.txt -p 0
