CXX = g++
CXXFLAGS = -std=c++17 -Wall -I./src
SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:.cpp=.o)
TARGET = cpp_sql_server

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
    $(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp
    $(CXX) $(CXXFLAGS) -c $< -o $@

test:
    $(MAKE) -C tests

clean:
    rm -f $(SRC_DIR)/*.o $(TARGET)
    $(MAKE) -C tests clean