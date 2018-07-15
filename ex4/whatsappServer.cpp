

//#define _GLIBCXX_USE_CXX11_ABI 0

#include "whatsappServer.h"

/**
 * Create a new Server object
 * @param i_port  port to connect to clients
 */
whatsappServer::whatsappServer(unsigned short i_port)
{
    port = i_port;
    server_main();
}


/**
 * Read message from socket into buffer
 */
int whatsappServer::read_socket(int socket)
{
    int br;
    int all_br;
    br = 0;
    all_br = 0;

    memset(buffer, '\0', MAX_BUFFER);
    while (all_br < MAX_BUFFER)
    {
        br = read(socket, ((char *) buffer + all_br), MAX_BUFFER - all_br);
        if (br > 0)
        {
            all_br += br;
        }
        if (br < 1)
        {
            print_error("read()", errno);
            exit(1);
        }

    }
    return (all_br);

}


/**
 * This function builds the FD sets
 */
void whatsappServer::build_fds()
{

    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO,&read_fds);
    FD_SET(server_sock_fd, &read_fds);
    max_fd = server_sock_fd;

    for (const auto &map_pair:fd_name_map)
    {
        if (map_pair.second > 0)
        {
            FD_SET(map_pair.second, &read_fds);

        }
        if (map_pair.second > max_fd)
        {
            max_fd = map_pair.second;
        }

    }

}


/**
 * Function reads input from the keyboard.
 */
void whatsappServer::read_stdin()
{
    char stdinread[MAX_BUFFER];
    memset (stdinread,'\0',MAX_BUFFER);
    fgets(stdinread,MAX_BUFFER,stdin);
    std::string str_stdin_read = std::string(stdinread);
    if(!str_stdin_read.compare("EXIT\n")){
        print_exit();
        exit(0);
    }
    else{
        print_invalid_input();
    }

}


/**
 * This function runs the server.
 */
void whatsappServer::server_main()
{
    /* Set nonblock for stdin. */
    int flag = fcntl(STDIN_FILENO, F_GETFL, 0);
    flag |= O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flag);
    errno=0;

    exit_flag = false;
    int ready_fd_num;
    std::string buff_str;
    int incoming_socket;
    int addlen = sizeof(sa);
    createSocket();

    // run as long as we have not exited
    while (!exit_flag)
    {
        if(errno != 0){
            print_error("client_main()",  errno);
            exit(1);
        }
        build_fds();
        //wait for an activity on one of the sockets, timeout is NULL, so wait
        // indefinitely (until "exit" command)
        ready_fd_num = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if ((ready_fd_num < 1) && (errno != EINTR))
        {
            // no ready sets - undefined error - not a valid state
            print_error("select()", errno);
            exit(1);
        }
        if(FD_ISSET(STDIN_FILENO,&read_fds)){
            read_stdin();
        }

        // handle new client
        if (FD_ISSET(server_sock_fd, &read_fds))
        {
            if ((incoming_socket = accept(server_sock_fd, (struct sockaddr *) &sa,
                                          (socklen_t *) &addlen)) < 0)
            {
                // error
                print_error("select()", errno);
                exit(1);
            }

            // read data from socket, and parse it
            read_socket(incoming_socket);
            buff_str = std::string(buffer);

            // check if new client name exists.
            // if valid, "accept it" and "poke" the read FD set
            if (fd_name_map.count(buff_str) == 0)
            {
                bool inGroup= false;
                for (unsigned int i = 0; i < group_list.size(); i++)
                {
                    if (!group_list.at(i).at(0).compare(buff_str))
                    {
                        inGroup = true;
                        writeMessage("", dup_name,incoming_socket);
                        break;
                    }
                }
                if(!inGroup){
                    fd_name_map[buff_str] = incoming_socket;
                    FD_SET(incoming_socket, &read_fds);
                    print_connection_server(buff_str);
                    writeMessage(buff_str, valid_name, -1);
                }


            }
            else
            {


                //otherwise, send a message back to the client indicating failure
                writeMessage("", dup_name,incoming_socket);
            }
            continue;
        }
        std::map<std::string,int> fd_name_map_cpy;
        fd_name_map_cpy.insert(fd_name_map.begin(),fd_name_map.end());

        // read all clients that their FD set is true (i.e. received new information)
        for (const auto &map_pair:fd_name_map_cpy)
        {
            if (map_pair.second > 0)
            {
                if (FD_ISSET(map_pair.second, &read_fds))
                {
                    read(map_pair.second, buffer, MAX_BUFFER);


                    commandHandler(map_pair.first);

                }

            }

        }

    }

}


/**
 * handle the different commands from the client.
 */
void whatsappServer::commandHandler(std::string client_name)
{
    std::string response;
    command_type commandT;
    std::string target_client = client_name;
    std::string name;
    std::string message;
    std::vector<std::string> clients;
    int fd_or_name = -1;

    // verify the buffer isn't empty:

     if(buffer[0]=='\0'){
         print_error("commandHandler",errno);
         exit(1);
     }
    // parse
    parse_command(std::string(buffer), commandT, name, message, clients);

    // handle the different commands:

    // handle WHO:
    if (commandT == WHO)
    {
        print_who_server(client_name);
        for (const auto &map_pair:fd_name_map)
        {
            response += (map_pair.first) + ",";
        }
        response.erase(response.begin() + response.size() - 1);
    }

    // handle "CREATE GROUP"
    else if (commandT == CREATE_GROUP)
    {
        response = "true";
        for (unsigned int i = 0; i < group_list.size(); i++)
        {
            if (!group_list.at(i).at(0).compare(name))
            {
                response = "false";
                break;
            }
        }
        for (const auto &map_pair:fd_name_map)
        {
            if (!map_pair.first.compare(name))
            {
                response = "false";
                break;
            }
        }
        // leave only one client name (erase duplicates)
        clients.push_back(client_name);
        std::sort(clients.begin(), clients.end());
        clients.erase(unique(clients.begin(), clients.end()), clients.end());
        for (unsigned int i = 0; i < clients.size(); i++)
        {
            if (fd_name_map.count(clients.at(i)) == 0)
            {
                response = "false";
                break;
            }
        }

        // if the response IS true: insert thegroup name, and then all participants
        if (!response.compare("true"))
        {
            clients.insert(clients.begin(), name);
            group_list.push_back(clients);
        }
        print_create_group(true,!response.compare("true"),client_name,name);
    }

    // handle EXIT command:
    else if (commandT == EXIT)
    {
        fd_or_name = fd_name_map[client_name];
        int offsetj = 0;
        int offseti = 0;
        response="true";
        fd_name_map.erase(client_name);

        for (unsigned int i = 0; i-offseti < group_list.size(); i++)
        {
            offsetj = 0;

            for(unsigned int j=1;j-offsetj<group_list.at(i-offseti).size();j++)
            {
                if(!group_list.at(i-offseti).at(j-offsetj).compare(client_name))
                {
                    group_list.at(i-offseti).erase( group_list.at(i-offseti).begin()+j-offsetj);
                    offsetj++;
                }
                if(group_list.at(i-offseti).size()<3){
                    group_list.at(i-offseti).clear();
                    group_list.erase(group_list.begin()+i-offseti);

                    offseti++;
                    break;
                }

            }

        }

        print_exit(true,client_name);
//        build_fds();
    }

    // handle SEND command:
    else if (commandT == SEND)
    {
        response = "false";

        std::string dest_message = client_name + ": " + message;
        // if the client name already exists:
        if (fd_name_map.count(name) == 1)
        {
            response = "true";
            target_client = name;
            writeMessage(name, dest_message,-1);
        }

        else
        {
            // verify there is no other group with this name:
            for (unsigned int i = 0; i < group_list.size(); i++)
            {
                if (!group_list.at(i).at(0).compare(name))
                {
                    response = "false";
                    for (unsigned long j = 1; j < group_list.at(i).size(); j++){
                        if (!group_list.at(i).at(j).compare(client_name))
                        {
                            response = "true";
                        }
                    }
                    if(!response.compare("false")){
                        break;
                    }
                    for (unsigned long j = 1; j < group_list.at(i).size(); j++)
                    {
                        if (group_list.at(i).at(j).compare(client_name))
                        {
                            writeMessage(group_list.at(i).at(j), dest_message,-1);
                        }
                    }
                    break;
                }

            }

        }
        print_send(true,!response.compare("true"),client_name,name,message);


    }


    writeMessage(client_name, response,fd_or_name);
    if(commandT==EXIT)
    {
//        close(fd_or_name);
//        FD_CLR(fd_or_name,&read_fds);
    }
}


/**
 * Write message to buffer
 */
void whatsappServer::writeMessage(std::string targetClient, std::string message,int client_fd){
    memset(buffer, '\0', MAX_BUFFER);
    for (unsigned long i = 0; (i < message.size() && i < MAX_BUFFER); i++)
    {
        buffer[i] = message.at(i);
    }
    if(client_fd<0)
    {
        write(fd_name_map[targetClient], buffer, MAX_BUFFER);
    }
    else{
        write(client_fd, buffer, MAX_BUFFER);
    }
}


/**
 * build a new socket
 */
void whatsappServer::createSocket()
{

    int opt = 1;
    //type of socket created
    // build host struct
    memset(&sa, 0, sizeof(sa));
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);

    //create a master socket
    if ((server_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        print_fail_connection();
        exit(EXIT_FAILURE);
    }
    // server socket allow multiple connections
    if (setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0)
    {
        print_fail_connection();
        exit(1);
    }

    if (bind(server_sock_fd, (struct sockaddr *) &sa, sizeof(sa)) < 0)
    {
        print_fail_connection();
        exit(EXIT_FAILURE);
    }
    if (listen(server_sock_fd, 10) < 0)
    {
        print_fail_connection();
        exit(1);
    }

}


/**
 * Main function
 */
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        print_server_usage();
        return 1;
    }
    else
    {
        unsigned short port = checkValidPort(argc, argv, 1,true);
        whatsappServer whatsappServer_obj = whatsappServer(port);
    }
    return 0;
}
