
Compile the code using the provided makefile:

    make

To clear the directory (and remove .txt files):
   
    make clean

To start the program:

    ./start.sh

To run the client  (must use port 9000 as that is the coordinator port)

    ./client -a host_addr -p 9000 -c id

To kill the program:

    ./kill.sh