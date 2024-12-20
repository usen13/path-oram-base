# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -Wall -Wextra	-g

# Target executable
TARGET = shamir_parser

# Source files
SRCS = shamir_parser.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link the target executable
$(TARGET): $(OBJS)
	$(CXX)	$(CXXFLAGS)	-o	$(TARGET)	$(OBJS)

# Compile source files into object files
%.o: %.cpp
	$(CXX)	$(CXXFLAGS)	-c	$<	-o	$@

# Clean up build files
clean:
	rm	-f	$(TARGET)	$(OBJS)	server_*_tuple_*.txt

# Phony targets
.PHONY: all clean