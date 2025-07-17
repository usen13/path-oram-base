CXX = g++
CXXFLAGS = -std=c++17 -Wall -I./src -I/usr/include/nlohmann -I../Shamir_Parser	-I./tests
GTEST_LIBS = -lgtest -lgtest_main -pthread
SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:.cpp=.o)
TARGET = cpp_sql_server
BDIR = bin

SQLHANDLER_TEST_SRC=tests/sql_handler_test.cpp
SQLHANDLER_TEST_BIN=$(BDIR)/test-sql_handler

$(SQLHANDLER_TEST_BIN): $(OBJS) $(SQLHANDLER_TEST_SRC)
	$(CXX)	-o $@ $^	$(CXXFLAGS)	$(INCLUDES)	$(LDLIBS)	$(LDFLAGS)	$(GTEST_LIBS)

run-sql_handler_test: $(SQLHANDLER_TEST_BIN)
	$(SQLHANDLER_TEST_BIN)

.PHONY: run-sql_handler_test clean-sql_handler_test
.PHONY: all clean test debug

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
	
clean-sql_handler_test:
	$(RM)	$(SQLHANDLER_TEST_BIN)