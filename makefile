
main: main.o read_bmp.o write_bmp.o grayscale.o flip.o
	gcc -Wall -g main.o read_bmp.o write_bmp.o grayscale.o flip.o -lm -o main

main.o: main.c read_bmp.h write_bmp.h grayscale.h flip.h
	gcc -Wall -g -c main.c

read_bmp.o: read_bmp.c read_bmp.h 
	gcc -Wall -g -c read_bmp.c

write_bmp.o: write_bmp.c write_bmp.h
	gcc -Wall -g -c write_bmp.c

grayscale.o: grayscale.c grayscale.h
	gcc -Wall -g -c grayscale.c

flip.o: flip.c flip.h
	gcc -Wall -g -c flip.c

clean:
	rm -f main *.o

remove:
	rm -f grayscale.bmp