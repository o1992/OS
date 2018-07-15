
#include "whatsappClient.h"

/**
 * Constructor for the Whatsapp Client
 * @param i_port  port to connect to server
 * @param dest_i_ip the ip of the server
 * @param i_clientName  client name
 */
whatsappClient::whatsappClient(unsigned short i_port, std::string dest_i_ip,std::string i_clientName){
    memset (buffer,'\0',MAX_BUFFER);
    dest_ip = dest_i_ip.c_str();
    port = i_port;
    clientName = std::move(i_clientName);
    if (!check_digits_and_letters_only(clientName)){
        print_fail_connection();
        exit(1);
    }
    exit_flag = false;
    response_flag = false;
    client_main();
    first_time = true;
}

/**
 * This function resets all the FD sets, and rebuilds them.
 * This function is called every time, so that the "select" function will only hit new incoming data.
 */
void whatsappClient::build_fds(){
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO,&read_fds);
    FD_SET(client_sock_fd,&read_fds);
    FD_ZERO(&write_fds);
}


/**
 * Write a request to the server.
 * @return the number of bytes written
 */
void whatsappClient::write_server_socket()
{
    int bytes_sent =(int)write(client_sock_fd, buffer,MAX_BUFFER);
    if(bytes_sent<0){
        print_send(false,false, nullptr, nullptr, nullptr);
    }
}


/**
 * Read a message from the server, sent in the server socket
 */
int whatsappClient::read_server_socket(){
    int br;
    int all_br;
    br=0;
    all_br=0;
    errno = 0;
    memset (buffer,'\0',MAX_BUFFER);

    // read from the socket into the buffer:
    while(all_br<MAX_BUFFER){
        br=read(client_sock_fd,((char*)buffer+all_br),MAX_BUFFER-all_br);
        if(br>0){

            all_br+=br;
        }
        if(br<1){
            print_exit();
            shutdown(client_sock_fd,SHUT_RDWR);
            close(client_sock_fd);
            exit(1);
        }
    }
    return(all_br);

}


/**
 * Read user input from the keyboard.
 * If invalid, return CommandT "INVALID", and otherwise give notice to the "read FD".
 */
command_type whatsappClient::read_stdin(){

    // receive message from stdin:
    memset (buffer,'\0',MAX_BUFFER);
    fgets(buffer,MAX_BUFFER,stdin);

    // erase "\n" from buffer, for parse handling:
    std::string str_buffer = std::string(buffer);
    str_buffer.erase(str_buffer.begin()+str_buffer.size()-1);
    buffer[str_buffer.size()] = '\0';
    if(str_buffer.size() == 0){
        print_invalid_input();
        FD_ZERO(&write_fds);
        return INVALID;
    }

    // parse the command and verify if invalid
    try{
        parse_command(str_buffer, commandT,name,message,clients);
    }
    catch(const std::exception &e){
        print_invalid_input();
        FD_ZERO(&write_fds);
        return INVALID;
    }


    if(commandT==INVALID){
        print_invalid_input();
        FD_ZERO(&write_fds);
        return INVALID;
    }

    // verify the name consists only of letters and digits
    if (!check_digits_and_letters_only(name)){
        if(commandT == SEND){
            print_send(false, false, clientName, name, message);
        }
        // or during create group
        else{
            print_create_group(false, false, clientName, name);
        }
        return INVALID;
    }
    // verify the clients consist only of letters and digits
    for (auto& client:clients){
        if (!check_digits_and_letters_only(client)){
            print_create_group(false, false, client, name);
            return INVALID;
        }
    }

    //  TIME TO VERIFY THE ACTUAL COMMANDS:

    // handle case of input for "create group" command:
    if(commandT==CREATE_GROUP)
    {
        // verify correction of command:
        // (i.e. there is at least one client, which is not the client himself)
        std::sort(clients.begin(), clients.end());
        clients.erase(unique(clients.begin(), clients.end()), clients.end());
        int clients_num = 1;
        for (unsigned int i = 0; i < clients.size(); i++)
        {
            if (clients.at(i) != clientName)
            {
                clients_num++;
            }
        }
        if (clients_num < 2)
        {
            FD_ZERO(&write_fds);
            print_create_group(false, false, clientName, name);
            return INVALID;
        }
    }

    // handle case of sending a message
    if(commandT==SEND){
        // if trying to send a message to yourself:
        if(name==clientName){
            FD_ZERO(&write_fds);
            print_send(false,false,clientName,name,message);
            return INVALID;
        }
    }

    // finally, if the command is valid - "poke" the write FD's for new data arriving
    if(commandT!=INVALID) {
        response_flag = true;
        FD_SET(client_sock_fd, &write_fds);
    }

    return SEND;
}


/**
 * This function runs the client's course. It is the main function of the client
 */
int whatsappClient::client_main(){

    /* Set nonblock for stdin. */
    int flag = fcntl(STDIN_FILENO, F_GETFL, 0);
    flag |= O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flag);
    errno=0;

    createSocket();

    while(!exit_flag)
    {
        if(errno != 0){
            print_error("client_main()",  errno);
            exit(1);
        }
        //init  FDS, set read from server & stdin write to server
        build_fds();

        // wait for new data to arrive from any source
        int ready_fd_num = select(client_sock_fd+1,&read_fds,&write_fds, nullptr, nullptr);
        // and then handle:
        switch (ready_fd_num)
        {
            case (-1):
                // error
                print_error("select()",  errno);
                exit(1);

            case(0):
                // no ready sets - undefined error - not a valid state
                print_error("select()",  errno);
                exit(1);

            default:
                if(FD_ISSET(STDIN_FILENO,&read_fds)){
                    read_stdin();
                }

                if(FD_ISSET(client_sock_fd, &write_fds)){
                    write_server_socket();
                }
                if(FD_ISSET(client_sock_fd, &read_fds)){
                    read_server_socket();
                    // different handle if a response from the server, or a message sent from the server
                    if(response_flag || first_time){
                        handle_response();
                        response_flag = false;
                    }
                    else{
                        printf("%s\n", buffer);
                    }
                }
        }
        }
    return 0;
}


/**
 * Handle a response from the server to a request or message from the client
 */
void whatsappClient::handle_response(){
//    errno=0;
    std::vector<std::string> all_clients;
    char c[WA_MAX_INPUT];
    char *saveptr =buffer;
    const char* s;

    // handle duplicate user attempt
    if(first_time){
        if(compare_buf_to_word(buffer, dup_name)){
            close(client_sock_fd);
            print_dup_connection();
            exit(1);
        }
        else{
            print_connection();
            first_time = false;
            return;
        }

    }


    // handle a "who" command:
    if(commandT == WHO){
        strcpy(c, buffer);
        s = strtok_r(c,",",&saveptr);
        all_clients.emplace_back(s);
        if(!s) {
            print_invalid_input();
            exit(1);
        }
        while((s = strtok_r(NULL, ",", &saveptr)) != NULL) {
            all_clients.emplace_back(s);
        }
        print_who_client(true, all_clients);
    }

    // handle a "create group" command:
    else if(commandT == CREATE_GROUP){
        bool ans = compare_buf_to_word(buffer, "true");
            print_create_group(false, ans, clientName, name);
    }

    // handle "send" command
    else if (commandT == SEND){
        bool ans = compare_buf_to_word(buffer, "true");
        print_send(false, ans, clientName, name, message);
    }
    else if(commandT == EXIT){
        bool ans = compare_buf_to_word(buffer, "true");
        if(ans)
        {
            print_exit(false, "");
            exit_flag = true;
            shutdown(client_sock_fd,SHUT_RDWR);
            close(client_sock_fd);
        }
        else{
            print_error("handle_response",errno);
            exit(1);
        }

    }

}


/**
 * This function creates a new socket, first thing after creation.
 */
void whatsappClient::createSocket(){
    struct sockaddr_in sa;
    struct hostent *hp;

    if ((hp= gethostbyname(dest_ip)) == NULL) {
        print_client_usage();
        exit(1);
    }
    // build host struct
    memset(&sa,0,sizeof(sa));
    memcpy((char *)&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons(port);

    if ((client_sock_fd = socket(hp->h_addrtype, SOCK_STREAM,0)) < 0)
    {
        print_fail_connection();
        exit(1);
    }
    // connect:
    if (connect(client_sock_fd, (struct sockaddr *)&sa , sizeof(sa)) < 0) {
        close(client_sock_fd);
        print_fail_connection();
        exit(1);
    }
    for(unsigned long i=0;(i<clientName.size() && i<MAX_BUFFER);i++){
        buffer[i]=clientName.at(i);
    }
    int bytes_sent =write(client_sock_fd,buffer,MAX_BUFFER);
    if(bytes_sent<0){
        print_fail_connection();
        exit(1);
    }
}


/**
 * This is the main function.
 */
int main(int argc, char** argv)
{
    if(argc!=4){
        print_client_usage();
        return 1;
    }
    else
    {
        unsigned short port = checkValidPort(argc, argv, 3,false);

        whatsappClient whatsappClientObj = whatsappClient(port, argv[2], argv[1]);
    }
    return 0;
}
