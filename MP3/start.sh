#!/bin/bash

if [ ! -d /logfiles ]; then
    mkdir logfiles;
fi;

if [ ! -d /master1_userfiles ]; then
    mkdir master1_userfiles;
fi;

if [ ! -d /master2_userfiles ]; then
    mkdir master2_userfiles;
fi;

if [ ! -d /master3_userfiles ]; then
    mkdir master3_userfiles;
fi;

if [ ! -d /slave1_userfiles ]; then
    mkdir slave1_userfiles;
fi;

if [ ! -d /slave2_userfiles ]; then
    mkdir slave2_userfiles;
fi;

if [ ! -d /slave3_userfiles ]; then
    mkdir slave3_userfiles;
fi;

sleep 0.1

./coordinator -p 9000 &

./server -a localhost -b 9000 -c 8080 -d 1 -e master &
./server -a localhost -b 9000 -c 8070 -d 1 -e slave &

./server -a localhost -b 9000 -c 8081 -d 2 -e master &
./server -a localhost -b 9000 -c 8071 -d 2 -e slave &
   
./server -a localhost -b 9000 -c 8082 -d 3 -e master &
./server -a localhost -b 9000 -c 8072 -d 3 -e slave &

./synchronizer -a localhost -b  9000 -c 9090 -d 1 &
./synchronizer -a localhost -b  9000 -c 9091 -d 2 &
./synchronizer -a localhost -b  9000 -c 9092 -d 3 &

sleep 0.3

