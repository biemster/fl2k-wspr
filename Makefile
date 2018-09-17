HEADERS = osmo-fl2k.h
INCLUDES=-I/usr/include/libusb-1.0/
LDFLAGS=-lusb-1.0 -pthread -lm

default: fl2k-wspr

fl2k-wspr.o: fl2k-wspr.cpp $(HEADERS)
	g++ -std=c++11 -c fl2k-wspr.cpp -o fl2k-wspr.o $(INCLUDES)

libosmo-fl2k.o: libosmo-fl2k.c $(HEADERS)
	gcc -c libosmo-fl2k.c -o libosmo-fl2k.o  $(INCLUDES)

fl2k-wspr: fl2k-wspr.o libosmo-fl2k.o
	g++ -ggdb fl2k-wspr.o libosmo-fl2k.o -o fl2k-wspr $(LDFLAGS)

clean:
	-rm -f fl2k-wspr.o
	-rm -f libosmo-fl2k.o
	-rm -f fl2k-wspr
