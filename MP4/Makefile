
CXX = gcc
CXXFLAGS = -pthread -g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable

all: clean client server

client: client.c utils.o queue.o
	    $(CXX) $(CXXFLAGS) client.c utils.o queue.o -o client

server: server.c utils.o queue.o
	$(CXX) $(CXXFLAGS) server.c utils.o queue.o -o server

utils.o: utils.c utils.h
	$(CXX) $(CXXFLAGS) -c utils.c -o $@

queue.o: queue.c queue.h
	  $(CXX) $(CXXFLAGS) -c queue.c -o $@



.PHONY : clean
clean:
	$(RM) -f node work monitor *~ *.ko *.o *.mod.c *.o.cmd Module.symvers modules.order
