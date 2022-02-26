#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <sys/select.h> 
#include <unordered_map>
#include <vector>
#include <set>
#include "interface.h"


using namespace std;


int main(int argc, char **argv)
{
    //for the server, we only need to specify a port number
    if(argc != 2)
    {
        fprintf(stderr,
				"usage: enter port number\n");
		exit(1);
    }
    //grab the port number
    int port = atoi(argv[1]);

    //buffer to send and receive messages with
    char msg[1500];
     
    //setup a socket and connection tools
    sockaddr_in servAddr;
    bzero((char*)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
 
    /*
        create the socket and get the 
        socket file discriptor for the server
    */
    int serverSd = socket(AF_INET, SOCK_STREAM, 0);

    //check if there is an error for the server socket
    if(serverSd < 0)
    {
        cerr << "Error establishing the server socket" << endl;
        exit(0);
    }

    //bind the socket to its local address
    int bindStatus = bind(serverSd, (struct sockaddr*) &servAddr, 
        sizeof(servAddr));

    //check if there is an error binding the socket
    if(bindStatus < 0)
    {
        cerr << "Error binding socket to local address" << endl;
        exit(0);
    }

    //listen up to 128 connections
    listen(serverSd, SOMAXCONN);

    /* 
        Create two sets of sockets one is to store 
        all the current sockets and they other ones for 
        new connections
    */ 
    fd_set current_sockets, ready_sockets;
    
    // Clear the the current sockests 
	FD_ZERO(&current_sockets);

    // set the server socket file discriptor in the current sockets
    FD_SET(serverSd, &current_sockets);

    //create a sock address to accept connections
    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);
   
   /* 
        chatrooms is a map that organizes each chatroom
        and stores all of the clients in their designated 
        room.

        @key string --- The name of the chatroom
        @value vector<int> --- all the clients in the chatroom
   */
    unordered_map<string, vector<int> > chatrooms;

    /* 
        clients is a map that stores that is used to 
        see which client is in each chat room. This is 
        useful when a client sends a chat the server needs
        to know which chatroom to send the message to

        @key int --- client ID
        @value string --- name of the clients chatroom
   */
    unordered_map<int, string > clients;

    /* 
        This keeps track of all the chatrooms used 
        for the list function so we can see all the 
        chatrooms

        @value string --- name of the chatroom
   */
    vector<string> listChatrooms;

    /* 
        This is a set of all of the clients that are in 
        chatmode. We want to use a set so we can take advantage
        of the find function to see if a client is or is not 
        int the set 

        @value int --- client ID
   */
    set<int> chatmodeClients;


    // The server will constantly run
    while(1)
    {
        //set the ready sockets to the current sockets
        ready_sockets = current_sockets;

		// Select a socket within the ready sockets
		int socketCount = select(FD_SETSIZE, &ready_sockets, nullptr, nullptr, nullptr);
		
        //Check to see if there is a select error
        if(socketCount < 0) { 
		    perror("Select error");
		    exit(1);
		}

        // loop through all of the slots in the set to see which socket we have selcted
        for(int i =0; i< FD_SETSIZE; i++){

            //if we are at the indeax 
            if(FD_ISSET(i, &ready_sockets)) {

                //save the socket to a variable for readbility 
                int selectedSock = i;

                //check if we have selected the served socket
                if(selectedSock == serverSd) {

                    //accept the new connection
                    int client = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize);
                    if(client < 0) {
                        cerr << "Error accepting request from client!" << endl;
                        exit(1);
                    }

                    //add the new connection to our current sockets set
                    FD_SET(client,&current_sockets);
   
                } 
                else {
                    /*
                        These are all of the client sockets !!
                    */

                    // create a variable for keeping track if a client is in chatmode
                    bool chatmode = false;

                    //Check to see if a client is in chatmode
                    if(chatmodeClients.find(selectedSock) != chatmodeClients.end()) chatmode = true;
                    
                    //clear the buffer
                    memset(&msg, 0, sizeof(msg));

                    /* 
                        store the bytes recieved in data 
                        and recieve the data from the client socket
                    */
                    int data = recv(selectedSock, (char*)&msg, sizeof(msg), 0);
                    
                    // store the message in a string
                    string chatMsg = string(msg,0,data);
                    
                    // if there is no message close it and clear it from the set
                    if(data<= 0){
                        
                        close(selectedSock);
                        FD_CLR(selectedSock,&current_sockets);

                    } 
                    else {
                        /*
                            Use a string to hold the status
                            0 - SUCCESS,
                            1 - FAILURE_ALREADY_EXISTS,
                            2 - FAILURE_NOT_EXISTS,
                            3 - FAILURE_INVALID,
                            4 - FAILURE_UNKNOWN 
                        */
                        string status = "";

                        //use a string to hold the list of chatrooms if prompted 
                        string list ="";
                        
                        string data="";
                        
                        //use a string to hold how many memebers in the chatroom
                        string num_member = "";

                        //adhere for lowercase or capitlization by using to uppercase
                        touppercase(msg, strlen(msg) - 1);

                        //check to see if it is a create command
                        if(strncmp(msg, "CREATE", 6) == 0 && !chatmode) {
                            //use char array to store the name of the chatroom
                            char name[150];

                            //copy the name into the name char array
                            memcpy(name, &msg[7], strlen(msg)+1);

                            // store the name into a string
                            string s(name);

                            // string s = "";
                            // for (int ind = 0; ind < strlen(msg)-7; ind++) {
                            //     s = s + name[ind];
                            // }
                            
                            //check if the chatroom does not exist
                            if (chatrooms.find(s) == chatrooms.end()) {
                                //create a vector to store clients in the chatroom 
                                vector<int> room;

                                //put the vector the map with the key being the name of the
                                chatrooms[s] = room;

                                //add the chatroom to the list of chatrooms
                                listChatrooms.push_back(s);

                                //set status to zero for SUCCESS
                                status = "0";

                                //set to zero since there are no memebers
                                num_member = "0";
                            } else {
                                //the chatroom already exists set status to 1 - FAILURE_ALREADY_EXISTS
                                status = "1";
                            }
                            
                        }
                        //check to see if it is a join command
                        else if(strncmp(msg, "JOIN", 4) == 0 && !chatmode) {
                            
                            // create a string to store the name of the chatroom 
                            string s(msg);
                            s = s.substr(5);

                            //check to see if the chatroom doesn't exist
                            if (chatrooms.find(s) == chatrooms.end()) {
                                //Chatroom doesnt exist set status to 2 - FAILURE_NOT_EXISTS
                                status = "2";
                            }
                            else {
                                //Chatroom does exist set status to 0 - SUCCESS
                                status = "0";
                    
                                // Store the client's chatroom in the map with their ID as the key
                                clients[selectedSock] = s;

                                //add the client to their chatrooms vector
                                chatrooms.at(s).push_back(selectedSock);

                                //update the num_member variable
                                num_member = to_string(chatrooms.at(s).size());

                                //insert the clients to chatmode set
                                chatmodeClients.insert(selectedSock);
                            }
                            
                        }

                        //check to see if it is a delete command
                        else if(strncmp(msg, "DELETE", 6) == 0 && !chatmode) {
                            //strore the name of the chatroom in a char array
                            char name[150];
                            memcpy(name, &msg[7], strlen(msg)+1);

                            //store the char array into a string
                            string s = "";
                            for (int ind = 0; ind < strlen(msg)-7; ind++) {
                                s = s + name[ind];
                            }
                            
                            //check to see if the chatroom does not exist
                            if (chatrooms.find(s) == chatrooms.end()) {
                                //Chatroom doesnt exist set status to 2 - FAILURE_NOT_EXISTS
                                status = "2";
                            } 
                            else {
                                //Chatroom does exist set status to 0 - SUCCESS
                                status = "0";

                                //store the delete message that will be sent to the chatroom
                                string bye = "Warning: the chatting room is going to be closed..";                     
                                
                                // loop through all the clients in the chatroom
                                for(int mem : chatrooms.at(s)) {

                                    //erase them from the chatmode set
                                    chatmodeClients.erase(mem);

                                    // send the goodbye message to everyone 
                                    if(mem != selectedSock) {
                                        send(mem, bye.c_str(), bye.size()+1, 0);
                                    }
                                }
                                
                                //loop through the chatrooms 
                                for (int ind =0; ind<listChatrooms.size(); ind++) {

                                    // check if the chatroom is the one being deleted
                                    if (listChatrooms.at(ind) == s) {

                                        //erase the chatroom from the list
                                        listChatrooms.erase(listChatrooms.begin()+ind);
                                        break;
                                    }
                                }

                                //erase the chatroom from the map
                                chatrooms.erase(s);   
                                
                                //erase the clients chatroom value and key
                                clients.erase(selectedSock);
                            }

                        }
                        //check to see if the command is a list command
                        else if(strncmp(msg, "LIST", 4) == 0 && !chatmode) {
                            // set status to L - Special case to signify succesful list command
                            status = "L";
                            //loop through the list of chatrooms and store the name into a string
                            for(int i=0; i<listChatrooms.size();i++) {
                                list+= listChatrooms.at(i)+",";
                            }
                        } 
                        else {
                            //check if user is in chatmode 
                            if(chatmode) {
                                //get the chatroom the client is in
                                string chatroom = clients.at(selectedSock);
                                
                                //loop through all the clients in the chatroom
                                for(int mem : chatrooms.at(chatroom)) {
                                    //send the message to everyone expecte the sender
                                    if(mem != selectedSock) {
                                        send(mem, chatMsg.c_str(), chatMsg.size()+1, 0);
                                    }
                                }
                            }
                            else {
                                //user is not in chatmode set status to 3 - FAILURE_INVALID
                                status = "3";
                            }
                        }
                        
                        // Lastly send the message such as status and port if not in chatmode
                        if(!chatmode) {
                            //check to see if its a list command
                            if(status=="L") {
                                // add list command to status
                                data = status + " " + list;
                            } else {
                                //send status and number of members in chatroom
                                data = status + " " + num_member;
                            }
                            //add the port to the begining
                            data = to_string(port)  + data;
                            
                            //clear the msg buffer and copy the data string
                            memset(&msg, 0, sizeof(msg)); 
                            strcpy(msg, data.c_str());

                            //send the data to the selected client
                            send(selectedSock, (char*)&msg, strlen(msg), 0);
                        }    
                    }
                }
               
            }

        }
    }
    
    //clear the current sockets set and close the server socket
    FD_CLR(serverSd, &current_sockets);
    close(serverSd);

    //close all the sockets in the current socket set
    for(int i=0; i<FD_SETSIZE; i++)
	{
        if(FD_ISSET(i, &current_sockets)) {
    		FD_CLR(i, &current_sockets);
    		close(i);
        }
		
	}

    return 0;   
}
