#include <iostream>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "sns.grpc.pb.h"
#include <thread>
#include <google/protobuf/util/time_util.h>
#include "client.h"
#include <fstream>
#include <sstream>

using csce438::SNSService;

using grpc::Status;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using grpc::ClientContext;
using google::protobuf::Timestamp;
using namespace google::protobuf;

class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
    private:
        std::string hostname;
        std::string username;
        std::string port;
        std::unique_ptr< SNSService::Stub> server_stub;
        std::unordered_set <std::string> lines;

        
        // You can have an instance of the client stub
        // as a member variable.
        //std::unique_ptr<NameOfYourStubClass::Stub> stub_;
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }
    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------
	auto channel = grpc::CreateChannel(hostname+":"+port, grpc::InsecureChannelCredentials());
    server_stub = SNSService::NewStub(channel);
    Reply reply;
    Request request;
    ClientContext context;
    request.set_username(username);
    server_stub->Login(&context,request,&reply);
    
    if(reply.msg() != "SUCCESS") return -1;
    if(server_stub) return 1;


    return -1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Follow" service method for FOLLOW command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Follow(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------
    IReply ire;
    Reply reply;
    Request request;
    ClientContext context;
    
    if(strncmp(input.c_str(), "LIST", 4) == 0) {
        request.set_username(username);
        Status status = server_stub->List(&context, request,&reply);
        ire.grpc_status = status;
        ire.comm_status = SUCCESS;
        for(int i=0; i<reply.all_users_size(); i++) ire.all_users.push_back(reply.all_users(i));
        
        for(int i=0; i<reply.following_users_size(); i++) ire.following_users.push_back(reply.following_users(i));
	}
	else if(strncmp(input.c_str(), "FOLLOW", 6) == 0) {
	    char follow[150];
	    memcpy(follow, &input.c_str()[7], strlen(input.c_str())+1);
	    
	    std::string s(follow);
	    request.set_username(username);
	    request.add_arguments(s);
	    Status status = server_stub->Follow(&context, request,&reply);
        ire.grpc_status = status;
        if(reply.msg() == "FAILURE_NOT_EXISTS") {
            ire.comm_status = FAILURE_NOT_EXISTS;
        }
        else if (reply.msg() == "FAILURE_INVALID"){
            ire.comm_status = FAILURE_INVALID;
        } else {
            ire.comm_status = SUCCESS;
        }
	} 
	else if(strncmp(input.c_str(), "UNFOLLOW", 8) == 0) {
	    char unfollow[150];
	    memcpy(unfollow, &input.c_str()[9], strlen(input.c_str())+1);
	    
	    std::string s(unfollow);
	    request.set_username(username);
	    request.add_arguments(s);
	    Status status = server_stub->UnFollow(&context, request,&reply);
        ire.grpc_status = status;
        
        if(reply.msg() == "FAILURE_NOT_EXISTS") {
            ire.comm_status = FAILURE_NOT_EXISTS;
        }
        else if (reply.msg() == "FAILURE_INVALID"){
            ire.comm_status = FAILURE_INVALID;
        } else {
            ire.comm_status = SUCCESS;
        }
	}else if(strncmp(input.c_str(), "TIMELINE", 8) == 0) {
	    ire.grpc_status = Status::OK;
	    ire.comm_status = SUCCESS;
	}
    return ire;
}

void Client::processTimeline()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
	ClientContext context;
    std::shared_ptr<ClientReaderWriter<csce438::Message, csce438::Message> > stream(
        server_stub->Timeline(&context));
        
    
    
    std::thread writer([stream,this]() {
        while(true) {
            csce438::Message post;
            std::string post_msg;
            std::getline(std::cin, post_msg);
            
            auto timestamp = new Timestamp;
            timestamp->set_seconds(time(NULL));
            timestamp->set_nanos(0);
            
            post.set_username(username);
            post.set_msg(post_msg);
            post.set_allocated_timestamp(timestamp);
            
            stream->Write(post);
        }
      
      stream->WritesDone();
    });
    
    std::thread reader([stream,this]() {
        csce438::Message feed_messages;
        while(true) {
            // while (stream->Read(&feed_messages)) {
            //     std::cout << google::protobuf::util::TimeUtil::ToString(feed_messages.timestamp()) << " "<< feed_messages.username() << " >>" << feed_messages.msg() << std::endl;
            //     std::cout<<std::endl;
            // }
            std::ifstream userRead;
            std::stringstream ss;
            std::string temp;
            userRead.open(username+".txt");
            if(userRead.is_open()) {
                std::string line;
                bool firstTime = false;
                if(lines.size() == 0) {
                    firstTime = true;
                }
                 while(getline(userRead, line)) {
                    if(lines.find(line) == lines.end()) {
                        lines.insert(line);
                        ss.str(line);
                        getline(ss,temp,' ');
                        std::string date = temp;
                        
                        getline(ss,temp,' ');
                        std::string user = temp;
                        
                        getline(ss,temp);
                        std::string message = temp;
                        if(username == user ) {
                            if(firstTime) {
                                std::cout << user << "(" << date << ")" << " >> " << message << std::endl;
                                std::cout<<std::endl;
                            }
                            
                        } else {
                            std::cout << user << "(" << date << ")" << " >> " << message << std::endl;
                            std::cout<<std::endl;
                        }
                        
                    } 
                    ss.clear();
                    
                 }
                 firstTime = false;
            }
        }
    });
    
    reader.join();
    writer.join();
    // Status status = stream->Finish();
    // if (!status.ok()) {
    //   std::cout << "RPC failed." << std::endl;
    // }
}
