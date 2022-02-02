crc: crc.o 
	g++ crc.o -o crc
crc.o: crc.cpp
	g++ -pthread -c crc.cpp

crsd: crsd.o 
	g++ crc.o -o crc
crsd.o: crsd.cpp
	g++ -c crsd.cpp

clean:
	rm *.o crc crsd

