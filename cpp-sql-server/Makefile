CXX = g++
CXXFLAGS = -std=c++17 -Wall -I./src -I/usr/include/nlohmann -I../Shamir_Parser
SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:.cpp=.o)
TARGET = cpp_sql_server

.PHONY: all clean test debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS += -g -O0
debug: clean all

test:
#	$(MAKE) -C tests

clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)
	@if [ -f tests/Makefile ]; then $(MAKE) -C tests clean; fi
	rm -rf ../Shamir_Search_Results