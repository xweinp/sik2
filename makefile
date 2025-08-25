CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -Wshadow \
			  -Wnon-virtual-dtor -Woverloaded-virtual \
			  -Wconversion -O2

TARGETS = approx-client approx-server

.PHONY: all clean

all: $(TARGETS)



approx-client: client/approx-client.o client/utils-client.o common/utils.o
	$(CXX) $(CXXFLAGS) $^ -o $@

approx-server: server/approx-server.o server/utils-server.o common/utils.o
	$(CXX) $(CXXFLAGS) $^ -o $@


client/approx-client.o: client/approx-client.cpp client/utils-client.hpp common/utils.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

server/approx-server.o: server/approx-server.cpp server/utils-server.hpp common/utils.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

client/utils-client.o: client/utils-client.cpp client/utils-client.hpp common/utils.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

server/utils-server.o: server/utils-server.cpp server/utils-server.hpp common/utils.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

common/utils.o: common/utils.cpp common/utils.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f client/*.o server/*.o common/*.o $(TARGETS)
	

.PHONY: all clean debug