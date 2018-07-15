CC = g++
CFLAGS = -std=c++11

VALG =valgrind --max-stackframe=6860756249999999 --leak-check=full --show-possibly-lost=yes --show-reachable=yes --undef-value-errors=yes 

CLIENT_FILES = whatsappClient.cpp whatsappio.cpp whatsappClient.h
SERVER_FILES = whatsappServer.cpp whatsappio.cpp whatsappServer.h
HEADER_FILES = whatsappio.h common.h

CXXFLAGS = -Wall -std=c++11
ALL_FILES = $(CLIENT_FILES) $(SERVER_FILES) $(HEADER_FILES) README Makefile

all: tar client server

client: $(CLIENT_FILES) $(HEADER_FILES)
	$(CC) $(CXXFLAGS) $(CLIENT_FILES) -o whatsappClient
	
server: $(SERVER_FILES) $(HEADER_FILES)
	$(CC) $(CXXFLAGS) $(SERVER_FILES) -o whatsappServer
	
clean:
	rm -rf *.o
	
tar: $(ALL_FILES)
	tar -cvf ex4.tar $(ALL_FILES)
