//
// Created by omerrubi on 6/13/18.
//

#ifndef OS_EX4_WHATSAPPSERVER_H
#define OS_EX4_WHATSAPPSERVER_H
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
#include <map>


class whatsappServer
{

public:
    explicit whatsappServer(unsigned short port);

private:
    void writeMessage(std::string targetClient, std::string message,int client_fd);
    struct sockaddr_in sa;
    struct hostent *hp;
    unsigned short port ;
    int server_sock_fd;
    bool exit_flag;
    fd_set read_fds;
    fd_set write_fds;
    char buffer[MAX_BUFFER];
    void createSocket();
    void server_main();
    void build_fds();
    int max_fd;
    int read_socket(int socket);
    std::map <std::string, int> fd_name_map; // key:client-name , value: fd_num
    void commandHandler(std::string client_name);
    std::vector<std::vector<std::string>> group_list;
    void read_stdin();

};


#endif //OS_EX4_WHATSAPPSERVER_H
