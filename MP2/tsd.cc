#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <chrono>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
std::unordered_map <std::string, std::vector<Message>> users_feed; 
std::unordered_set <std::string> usernames;
std::unordered_set <std::string> active_users;
// std::vector<std::string> usernames;
// std::vector<std::string> active_users;
std::unordered_map<std::string, std::vector<std::string> > following;
std::unordered_map<std::string, std::vector<std::string> > followers;
std::unordered_map<std::string, bool > inTimeline;
std::mutex mu_;

class SNSServiceImpl final : public SNSService::Service {
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    
    

   
    
    for(std::string str : usernames)  reply->add_all_users(str);
    
    for(std::string s : following[request->username()])  reply->add_following_users(s);
    
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------

    std::string username = request->username();
    std::string follow = request->arguments(0);
    if(usernames.find(follow) == usernames.end()) {
      reply->set_msg("FAILURE_NOT_EXISTS");
      return Status::OK;
    }
    else if(username == follow) {
      reply->set_msg("FAILURE_INVALID");
      return Status::OK;
    }
    
    std::ofstream fout;
    fout.open("users.txt", std::ios_base::app | std::ios_base::ate);
    fout<<"follow: " << username <<  " " << follow << std::endl;
    fout.close();
    
    if (following.find(username) == following.end()) {
      std::vector<std::string> v;
      v.push_back(follow);
      following[username] = v;
    }
    else {
      following[username].push_back(follow);
    }
    
    if (followers.find(follow) == followers.end()) {
      std::vector<std::string> v;
      v.push_back(username);
      followers[follow] = v;
    } 
    else {
      followers[follow].push_back(username);
    }
  
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    std::string username = request->username();
    std::string unfollow = request->arguments(0);
    
    if(usernames.find(unfollow) == usernames.end()) {
      reply->set_msg("FAILURE_NOT_EXISTS");
      return Status::OK;
    }
    else if(username == unfollow) {
      reply->set_msg("FAILURE_INVALID");
      return Status::OK;
    }
    
    std::ofstream fout;
    fout.open("users.txt", std::ios_base::app | std::ios_base::ate);
    fout<<"unfollow: " << username <<  " " << unfollow << std::endl;
    fout.close();
    
    int index =-1;
    for (int i=0; i< following[username].size(); i++ )
    {
      if(following[username].at(i) == unfollow) {
        index = i; 
        break;
      }
    }
    
    following[username].erase(following[username].begin()+index);
    
    index=-1;
    for (int i=0; i< followers[unfollow].size(); i++ )
    {
      if(followers[unfollow].at(i) == username) {
        index = i; 
        break;
      }
    }
    followers[unfollow].erase(followers[unfollow].begin()+index);

    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    std::string username = request->username();
    //std::cout<<username<<std::endl;
 
    if(active_users.find(username) != active_users.end()) reply->set_msg("FAILURE_INVALID_USERNAME");
    else {
      reply->set_msg("SUCCESS");
      bool exists=false;
      for(std::string str : usernames) {
        if(str == username) exists = true;
      }
      if(!exists) {
        usernames.insert(username);
        
        std::ofstream fout;
        fout.open("users.txt", std::ios_base::app | std::ios_base::ate);
        fout<<"newuser: " << username << std::endl;
        fout.close();
        
        std::cout<<"username: " << username << "\n";
      }
      active_users.insert(username);
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    
    Message readMsg;
    Message send;
    
    while (stream->Read(&readMsg)) {
      if(inTimeline[readMsg.username()] == false) {
        inTimeline[readMsg.username()] = true;
        std::ifstream fin;
        std::stringstream ss;
        std::string username = readMsg.username();
        fin.open(username+".txt");
        
        if(fin.is_open()) {
          std::string line;
          std::string temp;
          while(getline(fin,line)) {
            std::cout << line << std::endl;
            Message send;
            auto timestamp = new Timestamp;
            ss.str(line);

            getline(ss,temp,' ');
            google::protobuf::util::TimeUtil::FromString(temp,timestamp);
            send.set_allocated_timestamp(timestamp);
            
            getline(ss,temp,' ');
            send.set_username(temp);
            
            getline(ss,temp);
            send.set_msg(temp);
            
            stream->Write(send);
            ss.clear();
            
          }
          fin.close();
        }
      }
      if(readMsg.msg().length() < 1) {
        std::cout << "pressed enter" << std::endl;
        continue;
      }
      std::cout << google::protobuf::util::TimeUtil::ToString(readMsg.timestamp()) << " " << readMsg.username() << " " << readMsg.msg() << std::endl;
      
      std::ofstream self_writer;
      self_writer.open(readMsg.username()+".txt", std::ios_base::app | std::ios_base::ate);
      self_writer << google::protobuf::util::TimeUtil::ToString(readMsg.timestamp()) << " " << readMsg.username() << " " << readMsg.msg() << std::endl;
      self_writer.close();
      for(std::string follower : followers[readMsg.username()]) {
        std::ofstream writer;
        writer.open(follower+".txt", std::ios_base::app | std::ios_base::ate);
        writer << google::protobuf::util::TimeUtil::ToString(readMsg.timestamp()) << " " << readMsg.username() << " " << readMsg.msg() << std::endl;
        writer.close();
        
        
        if(users_feed.find(follower) == users_feed.end()) {
          std::vector<Message> v;
          v.push_back(readMsg);
          users_feed[follower] = v;
        } else {
          users_feed[follower].push_back(readMsg);
        }
      }
      
    }
    for (auto msg : users_feed[readMsg.username()]) {
      stream->Write(msg);
      std::cout << "writing to feed " << msg.msg() << std::endl;
    }
    std::vector<Message> vNew;
    users_feed[readMsg.username()] = vNew;
        //std::ofstream writer;
        //writer.open(username+".txt", std::ios_base::app | std::ios_base::ate);
    
    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  SNSServiceImpl service;
  
  std::ifstream usersRead;
  std::stringstream ss;

  
  usersRead.open("users.txt");
  if(usersRead.is_open()) {
    std::string line;
    std::string temp;
    while(getline(usersRead, line)) {
      ss.str(line);
      while(getline(ss,temp,' ')) {
        if(temp=="newuser:") {
          getline(ss,temp);
          std::string username = temp;
          usernames.insert(username);
          inTimeline[username] = false;
        }
        else if(temp == "follow:") {
          getline(ss,temp, ' ');
          std::string user1 = temp;
          
          getline(ss,temp, ' ');
          std::string user2 = temp;
          std::cout << "follow " << user1 << " " << user2 << std::endl;
          
          if (following.find(user1) == following.end()) {
            std::vector<std::string> v;
            v.push_back(user2);
            following[user1] = v;
          }
          else {
            following[user1].push_back(user2);
          }
          
          if (followers.find(user2) == followers.end()) {
            std::vector<std::string> v;
            v.push_back(user1);
            followers[user2] = v;
          } 
          else {
            followers[user2].push_back(user1);
          }
        }
        else if(temp == "unfollow:") {
          getline(ss,temp, ' ');
          std::string user1 = temp;
          
          getline(ss,temp, ' ');
          std::string user2 = temp;
          
          int index =-1;
          for (int i=0; i< following[user1].size(); i++ )
          {
            if(following[user1].at(i) == user2) {
              index = i; 
              break;
            }
          }
          
          following[user1].erase(following[user1].begin()+index);
          
          index=-1;
          for (int i=0; i< followers[user2].size(); i++ )
          {
            if(followers[user2].at(i) == user1) {
              index = i; 
              break;
            }
          }
          followers[user2].erase(followers[user2].begin()+index);
        }
      }
      ss.clear();
    }
    usersRead.close();
    
  }
  
  std::string server_address("0.0.0.0:"+port_no);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
  return 0;
}
