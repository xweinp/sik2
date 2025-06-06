CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -Wshadow \
			  -Wnon-virtual-dtor -Woverloaded-virtual \
			  -Wconversion -O2

TARGETS = approx-client approx-server

.PHONY: all clean

all: $(TARGETS)


approx-client: approx-client.o utils-client.o utils.o
	$(CXX) $(CXXFLAGS) $^ -o $@
approx-server: approx-server.o utils-server.o utils.o
	$(CXX) $(CXXFLAGS) $^ -o $@

approx-client.o: approx-client.cpp utils-client.hpp utils.hpp
approx-server.o: approx-server.cpp utils-server.hpp utils.hpp

utils-client.o: utils-client.cpp utils.hpp
utils-server.o: utils-server.cpp utils.hpp

utils.o: utils.cpp utils.hpp

clean: 
	rm -f *.o $(TARGETS)
	

.PHONY: all clean debug