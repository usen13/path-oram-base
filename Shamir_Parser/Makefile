# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -g

# Linker flags
LDFLAGS=-L $(LDIR)	-lgtest -lgtest_main -pthread -lsodium -lgmp -lgmpxx

# Target executable
TARGET = shamir_parser

# Source files
SRCS = shamir_parser.cpp

# Header files
HDRS = shamir_parser.h

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

# Link the target executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(TARGET) $(OBJS)
	find ../shares -maxdepth 1 -type f -name 'server_*.txt' -exec rm -f {} +
#server_*.txt

# Phony targets
.PHONY: all clean