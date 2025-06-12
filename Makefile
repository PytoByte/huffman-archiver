all:
	gcc -o huf main.c archiver.c huff/node.c huff/tree/builder.c huff/tree/codes.c buffio.c progbar.c linkedlist.c filetools.c -g