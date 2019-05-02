# forgive the sucky Makefile, but I've never bothered to figure them out.

all: ringconnectd ringtime

ringconnectd: ringconnectd.o opt.o log.o
	gcc -Wall -O3 -o ringconnectd ringconnectd.o opt.o log.o

ringconnectd.o: ringconnectd.c opt.h
	gcc -Wall -O3 -c -o ringconnectd.o ringconnectd.c

opt.o: opt.c opt.h
	gcc -Wall -O3 -c -o opt.o opt.c

opt.h:

log.o: log.c
	gcc -Wall -O3 -c -o log.o log.c

ringtime: ringtime.c
	gcc -Wall -O3 -o ringtime ringtime.c

clean:
	rm -f ringconnectd ringtime *.o
