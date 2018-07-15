//
// Created by omerrubi on 6/12/18.
//

#ifndef OS_EX4_WHATSAPPCLIENT_H
#define OS_EX4_WHATSAPPCLIENT_H
#include "whatsappio.h"
#include "common.h"
#include<string>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <algorithm>

unsigned short checkValidPort(int argc, char** argv);
struct hostent;

class whatsappClient
{
public:
    whatsappClient(unsigned short i_port, std::string dest_i_ip, std::string i_clientName);


private:
    const char* dest_ip ;
    unsigned short port ;
    int client_sock_fd;
    std::string clientName ;
    int client_main();
    void createSocket();
    bool exit_flag;
    void build_fds();
    fd_set read_fds;
    fd_set write_fds;
    command_type read_stdin();
    char buffer[MAX_BUFFER];
    int read_server_socket();
    void write_server_socket();
    void handle_response();
    command_type commandT;
    std::string name;
    std::string message;
    std::vector<std::string> clients;
    bool response_flag;
    bool first_time;

};


#endif //OS_EX4_WHATSAPPCLIENT_H
