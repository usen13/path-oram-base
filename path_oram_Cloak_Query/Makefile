# definitions

SDIR=src
TDIR=test
HDIR=benchmark
IDIR=include
ODIR=obj
LDIR=lib
BDIR=bin

CXXFLAGS= -I/usr/include/gtest -I/usr/include/gtest  -pthread	-g

LDFLAGS=-L $(LDIR) -L /usr/include/openssl -lgtest -lgtest_main -pthread -lsodium -g

LDLIBS=-l boost_system -l ssl -l crypto # libs for main code
#REDISLIBS=-l redis++ -l hiredis # libs for redis support
#AEROSPIKELIBS=-l aerospike -l dl -l z # libs for aerospike support
#LDTESTLIBS=-l gtest -l pthread -l benchmark -l gmock # libs for tests and benchmarks

INCLUDES=-I $(IDIR) -I /usr/include/openssl
override CPPFLAGS += --std=c++17 -Wall -Wno-unknown-pragmas -fPIC
CC=g++
RM=rm -rf

#ifeq ($(filter -DUSE_REDIS=false,$(CPPFLAGS)),)
#	LDLIBS += $(REDISLIBS)
#endif

# ifeq ($(filter -DUSE_AEROSPIKE=false,$(CPPFLAGS)),)
# 	LDLIBS += $(AEROSPIKELIBS)
# endif

LIBNAME=pathoram

# if you follow the convention that for each class CLASS you have a header in
# $(IDIR)/CLASS.hpp, a code in $(SDIR)/CLASS.cpp and a test in $(TDIR)/test-CLASS.cpp,
# then the rest will magically work - it will compile each class and test and will run the tests.
# CLASS does not even have to be a class in C++.
ENTITIES = storage-adapter position-map-adapter utility oram stash-adapter

# dependencies - definitions plus header files
_DEPS = definitions.h $(addsuffix .hpp, $(ENTITIES))
DEPS = $(patsubst %, $(IDIR)/%, $(_DEPS))

_OBJ = $(addsuffix .o, $(ENTITIES))
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

TESTS = $(ENTITIES)
TESTBIN = $(addprefix $(BDIR)/test-, $(TESTS))
JUNITS= $(foreach test, $(TESTS), bin/test-$(test)?--gtest_output=xml:junit-$(test).xml)

BENCHMARKS = storage-adapter oram utility
BENCHMARKSBIN = $(addprefix $(BDIR)/benchmark-, $(BENCHMARKS))

INTEGRATION = oram-big
INTEGRATIONBIN = $(addprefix $(BDIR)/test-, $(INTEGRATION))

TESTSSS = $(BDIR)/test-sss

TESTSQL = $(BDIR)/test-sql

TESTSER3 = $(BDIR)/test-ser3
TESTSER4 = $(BDIR)/test-ser4
TESTSER5 = $(BDIR)/test-ser5
TESTSER6 = $(BDIR)/test-ser6

TESTSUM = $(BDIR)/test-sum
TESTAVG = $(BDIR)/test-avg
TESTMAX = $(BDIR)/test-max
TESTMIN = $(BDIR)/test-min

ENTRYPOINTCC = $(CC) -o $@ $^ $(CPPFLAGS) $(INCLUDES) $(LDLIBS) $(LDTESTLIBS) $(LDFLAGS)

# flags-setting commands

all: shared docs

binaries: $(TESTBIN) $(INTEGRATIONBIN) $(BENCHMARKSBIN)
cleandebug: clean debug

debug: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug: binaries

profile: CPPFLAGS += -fprofile-arcs -ftest-coverage -fPIC -O0
profile: clean run-tests-junit

# executables

.SECONDEXPANSION:
$(TESTBIN): $(OBJ) $$(subst $$(BDIR), $(TDIR), $$@).cpp
	$(ENTRYPOINTCC)

.SECONDEXPANSION:
$(BENCHMARKSBIN): $(OBJ) $$(subst $$(BDIR), $(HDIR), $$@).cpp
	$(ENTRYPOINTCC)

.SECONDEXPANSION:
$(INTEGRATIONBIN): $(OBJ) $$(subst $$(BDIR), $(TDIR), $$@).cpp
	$(ENTRYPOINTCC)

$(TESTSSS): $(OBJ) $(TDIR)/test-sss.cpp
	$(ENTRYPOINTCC)

run-test-sss: $(TESTSSS)
	$(TESTSSS)

clean-test-sss:
	$(RM) $(TESTSSS)

debug-test-sss: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-sss: $(TESTSSS)

$(TESTSQL): $(OBJ) $(TDIR)/test-sql.cpp
	$(ENTRYPOINTCC)

run-test-sql: $(TESTSQL)
	$(TESTSQL)

clean-test-sql:
	$(RM) $(TESTSQL)

debug-test-sql: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-sql: $(TESTSQL)

$(TESTSER3): $(OBJ) $(TDIR)/test-ser3.cpp
	$(ENTRYPOINTCC)

run-test-ser3: $(TESTSER3)
	$(TESTSER3)

clean-test-ser3:
	$(RM) $(TESTSER3)

debug-test-ser3: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-ser3: $(TESTSER3)

$(TESTSER4): $(OBJ) $(TDIR)/test-ser4.cpp
	$(ENTRYPOINTCC)

run-test-ser4: $(TESTSER4)
	$(TESTSER4)

clean-test-ser4:
	$(RM) $(TESTSER4)

debug-test-ser4: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-ser4: $(TESTSER4)

$(TESTSER5): $(OBJ) $(TDIR)/test-ser5.cpp
	$(ENTRYPOINTCC)

run-test-ser5: $(TESTSER5)
	$(TESTSER5)

clean-test-ser5:
	$(RM) $(TESTSER5)

debug-test-ser5: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-ser5: $(TESTSER5)

$(TESTSER6): $(OBJ) $(TDIR)/test-ser6.cpp
	$(ENTRYPOINTCC)

run-test-ser6: $(TESTSER6)
	$(TESTSER6)

clean-test-ser6:
	$(RM) $(TESTSER6)

debug-test-ser6: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-ser6: $(TESTSER6)

$(TESTSUM): $(OBJ) $(TDIR)/test-sum.cpp
	$(ENTRYPOINTCC)

run-test-sum: $(TESTSUM)
	$(TESTSUM)

clean-test-sum:
	$(RM) $(TESTSUM)

debug-test-sum: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-sum: $(TESTSUM)

$(TESTAVG): $(OBJ) $(TDIR)/test-avg.cpp
	$(ENTRYPOINTCC)

run-test-avg: $(TESTAVG)
	$(TESTAVG)

clean-test-avg:
	$(RM) $(TESTAVG)

debug-test-avg: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-avg: $(TESTAVG)

$(TESTMAX): $(OBJ) $(TDIR)/test-max.cpp
	$(ENTRYPOINTCC)

run-test-max: $(TESTMAX)
	$(TESTMAX)

clean-test-max:
	$(RM) $(TESTMAX)

debug-test-max: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-max: $(TESTMAX)

$(TESTMIN): $(OBJ) $(TDIR)/test-min.cpp
	$(ENTRYPOINTCC)

run-test-min: $(TESTMIN)
	$(TESTMIN)

clean-test-min:
	$(RM) $(TESTMIN)

debug-test-min: CPPFLAGS += -g -DTESTING	-fdebug-prefix-map=$(PWD)=.
debug-test-min: $(TESTMIN)

shared-debug: CPPFLAGS += -g -DDEBUG
shared-debug: shared

shared: CPPFLAGS += -DSHARED -O3
shared: $(OBJ)
	$(CC) -shared $(LDLIBS) $(LDFLAGS) -o $(BDIR)/lib$(LIBNAME).so $(OBJ)

# Rule to compile source files into object files

$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CPPFLAGS) $(INCLUDES)

# commands

# some Linuxes do not respect -L flag during execution, so we can modify the system LD path
ldconfig:
	@echo $(shell pwd)/bin > /etc/ld.so.conf.d/pathoram-dev.conf
	@ldconfig

# will compile the test against the library (not objects and sources) and will run it
run-shared-lib: shared
	$(CC) -o $(BDIR)/test-shared-lib $(TDIR)/test-shared-lib.cpp -I $(IDIR) -L $(BDIR) -l $(LIBNAME) $(CPPFLAGS)
	$(BDIR)/test-shared-lib

run-stash-size: shared
	$(CC) -o $(BDIR)/test-stash-size $(TDIR)/test-stash-size.cpp -I $(IDIR) -L $(BDIR) -l $(LIBNAME) $(CPPFLAGS)
	$(BDIR)/test-stash-size

run-tests-junit: CPPFLAGS += -DTESTING
run-tests-junit: $(TESTBIN)
	$(subst ?, ,$(addsuffix &&, $(JUNITS))) echo Tests passed!

run-tests: CPPFLAGS += -DTESTING
run-tests: $(TESTBIN)
	$(addsuffix &&, $(TESTBIN)) echo Tests passed!

run-benchmarks: $(BENCHMARKSBIN)
	$(addsuffix &&, $(BENCHMARKSBIN)) echo Benchmarks completed!

run-integration: CPPFLAGS += -DTESTING
run-integration: $(INTEGRATIONBIN)
	$(addsuffix &&, $(INTEGRATIONBIN)) echo Tests passed!

coverage: profile
	gcovr -r . $(addprefix -f $(SDIR)/, $(addsuffix .cpp, $(ENTITIES))) --exclude-unreachable-branches
	mkdir -p coverage-html/
	gcovr -r . --html --html-details -o coverage-html/index.html
	gcovr -r . -x -o cobertura.xml

docs: clean-docs
	doxygen ../Doxyfile

clean: clean-docs clean-binaries
	$(RM) $(ODIR)/* *~ *.dSYM *.gcov *.gcda *.gcno *.bin coverage-html junit-*.xml cobertura.xml
	$(RM) ../backup

clean-docs:
	$(RM) ../docs

clean-binaries:
	$(RM) $(BDIR)/*

# phony

.PHONY: docs clean clean-docs clean-binaries coverage
.PHONY: profile debug cleandebug
.PHONY: binaries all shared
.PHONY: run-tests run-integration run-benchmarks run-shared-lib run-tests-junit
.PHONY: run-test-sss clean-test-sss debug-test-sss
