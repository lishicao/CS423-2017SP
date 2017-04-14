
CXX = gcc
CXXFLAGS = -pthread -std=c++0x -g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable

all: clean server client 

client: client.c utils.o
	    $(CXX) $(CXXFLAGS) client.c utils.o -o client

server: server.c utils.o 
	$(CXX) $(CXXFLAGS) server.c utils.o -o server

utils.o: utils.c utils.h
	$(CXX) $(CXXFLAGS) -c utils.c -o $@



.PHONY : clean
clean:
	$(RM) -f node work monitor *~ *.ko *.o *.mod.c *.o.cmd Module.symvers modules.order
