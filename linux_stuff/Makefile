all:	lcreader-server lcreader-client

CFLAGS = -O2 -Wall

lcreader-server:	lcreader-server.c
	${CC} -o $@ $^

lcreader-client:	lcreader-client.c
	${CC} -o $@ $^

clean:
	rm -f lcreader-server lcreader-client *.o

install:	#echo "Note that you could be superuser (root) to install this!"
	cp lcreader-server /opt/lcreader/
	cp lcreader-client /opt/lcreader/
