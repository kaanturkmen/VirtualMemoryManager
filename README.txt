Kaan Turkmen (72213) & Eren Yenigul (72290)

Part 1            
==================
To compile:
gcc -o part1 part1.c
To run:
./part1 BACKING_STORE.bin addresses.txt

Implementation:
In part 1, since logical address space and physical address space have the same size, we are able to do simple mapping
between BACKING_MEMORY.bin to main memory. However, in the real life, logical address space is much greater than the physical one which will be simulated in the part 2.

Part 2            
==================
To compile & run:
Use the makefile
Update -p from makefile to choose LRU or FIFO replacement

Implementation:

For both FIFO & LRU:

TLB add is first checking given frame is used by some other page. If so, replaces old page with the new given one.

For FIFO 

Same as part1 but uses replacement. Whenever all frames are in use, the program overwrites to the first ever allocated frame, goes on with the next one in a circular manner. 

For LRU

We implemented a linked list, with fields page numbers and age. When list_add is called, each entry in the list is aged by one. 

list_add adds the given page to the list. If page already exists in the list, its age is resetted to 0.

Replacment policy for LRU is done by selecting the page with the highest age. Since whenever a page is referrenced its age is resetted, we know that pages with the lower ages are recently referenced. list_remove pops the page with the highest age.