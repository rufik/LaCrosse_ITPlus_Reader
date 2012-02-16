all:	lcr-client

CFLAGS = -O2 -Wall

lcr-client:	lcr-client.c
	${CC} -o $@ $^

clean:
	rm -f lcr-client *.o

install:	#echo "Note that you could be superuser (root) to install this!"
	cp lcr-server.py /opt/lcr/
	cp lcr-client /opt/lcr/
