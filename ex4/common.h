
#ifndef OS_EX4_COMMON_H
#define OS_EX4_COMMON_H

#include <arpa/inet.h>
#include <stdexcept>

#define MAX_BUFFER 2048 // 2048 Larger then WA_MAX_INPUT
#define PORT_RANGE 65535
std::string dup_name = "duplicate_name";
std::string valid_name = "valid";

/**
 * Verify that the initial port input is a valid port
 */
unsigned short checkValidPort(int argc, char** argv,int port_pos,bool server){
    std::string port = argv[port_pos];
    int value=0;
    try
    {
        value = std::stoi(port.c_str(), NULL, 10);
        if(port.length()>5 || value>PORT_RANGE || value<=0){

            throw std::bad_exception();
        }
    }
    catch(const std::exception& e){
        if(server){
            print_server_usage();
        }
        else
        {
            print_client_usage();
        }
        exit(1);    // If a client fails to connect for any other reason it should print
                    // “Failed to connect the server” and exit(1)
    }
    return (unsigned short)value;
}


/**
 * Compare the buffer to a given word.
 */
bool compare_buf_to_word(char* buffer, std::string word){
    std::string buff_str = std::string(buffer);
    return(!buff_str.compare(word));
}


/**
 * verify that a given string consists only of characters and digits
 * @param to_check
 * @return true iff the string is "good"
 */
bool check_digits_and_letters_only(std::string to_check){
    for (unsigned int i=0; i<to_check.size(); i++) {
        if( isalnum(to_check.at(i)) == 0){
            return false;
        }
    }
    return true;
}


/**
 * Check validity of ip.
 */
void checkIP(int argc, char** argv,int ip_input){
    const char* ip = argv[ip_input];
    int is_valid = inet_pton(AF_INET, ip, &ip);
    if(is_valid == 0){
        print_client_usage();
        exit(1);
    }

}








#endif //OS_EX4_COMMON_H