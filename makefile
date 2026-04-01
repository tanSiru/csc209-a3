
main: main.o read_bmp.o filter.o 
	gcc -Wall -g main.o read_bmp.o filter.o -o main

main.o: main.c read_bmp.h filter.h
	gcc -Wall -g -c main.c

read_bmp.o: read_bmp.c read_bmp.h 
	gcc -Wall -g -c read_bmp.c

filter.o: filter.c filter.h
	gcc -Wall -g -c filter.c

clean:
	rm -f main *.o

remove:
	rm -f new_image.bmp