
main: main.o read_bmp.o grayscale.o 
	gcc -Wall -g main.o read_bmp.o grayscale.o -o main

main.o: main.c read_bmp.h grayscale.h
	gcc -Wall -g -c main.c

read_bmp.o: read_bmp.c read_bmp.h 
	gcc -Wall -g -c read_bmp.c

grayscale.o: grayscale.c grayscale.h
	gcc -Wall -g -c grayscale.c

clean:
	rm -f main *.o

remove:
	rm -f grayscale.bmp