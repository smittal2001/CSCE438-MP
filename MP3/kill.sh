#!/bin/bash

kill -9 $(pgrep -f './coordinator -p 9000' | head -1)

kill -9 $(pgrep -f './server -a localhost -b 9000 -c 8080 -d 1 -e master' | head -1)
kill -9 $(pgrep -f './server -a localhost -b 9000 -c 8070 -d 1 -e slave' | head -1)

kill -9 $(pgrep -f './server -a localhost -b 9000 -c 8081 -d 2 -e master' | head -1)
kill -9 $(pgrep -f './server -a localhost -b 9000 -c 8071 -d 2 -e slave' | head -1)

kill -9 $(pgrep -f './server -a localhost -b 9000 -c 8082 -d 3 -e master' | head -1)
kill -9 $(pgrep -f './server -a localhost -b 9000 -c 8072 -d 3 -e slave' | head -1)


kill -9 $(pgrep -f './synchronizer -a localhost -b  9000 -c 9090 -d 1' | head -1)
kill -9 $(pgrep -f './synchronizer -a localhost -b  9000 -c 9091 -d 2' | head -1)
kill -9 $(pgrep -f './synchronizer -a localhost -b  9000 -c 9092 -d 3' | head -1)