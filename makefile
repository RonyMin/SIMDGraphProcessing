# Uncomment the following line to compile without PCM support
#INTEL_PCM_HOME=/afs/cs.stanford.edu/u/noetzli/IntelPerformanceCounterMonitorV2.7

#CXX ?= g++-4.7
#CXX = /dfs/scratch0/noetzli/downloads/tmp/cilkplus-install/bin/g++
override CXXFLAGS += -mavx -std=c++0x -fopenmp -pedantic -O3 -Wall -Wextra -Wcast-align -march=native -mtune=native

INCLUDE_DIRS=-Isrc
OBJDIR=build
EXEDIR=bin
HEADERS=$(wildcard src/*hpp)
SOURCES=$(wildcard src/*cpp)
OBJECTS=$(SOURCES:src/%.cpp=$(OBJDIR)/%.o)

ifdef INTEL_PCM_HOME
	INCLUDE_DIRS+=-I$(INTEL_PCM_HOME)
	EXT_OBJECTS=$(INTEL_PCM_HOME)/cpucounters.o $(INTEL_PCM_HOME)/msr.o $(INTEL_PCM_HOME)/pci.o $(INTEL_PCM_HOME)/client_bw.o
endif

APPS_SOURCES=$(shell ls apps)
TOOLS_SOURCES=$(shell ls tools)
TESTS_SOURCES=$(shell find test -name '*.cpp' -exec basename {} \;)

APPS=$(APPS_SOURCES:.cpp=)
TOOLS=$(TOOLS_SOURCES:.cpp=)
TESTS=$(TESTS_SOURCES:.cpp=)

APPS_EXES=$(APPS:%=$(EXEDIR)/%)
TOOLS_EXES=$(TOOLS:%=$(EXEDIR)/%)
TESTS_EXES=$(TESTS:%=$(EXEDIR)/%)

all: $(APPS_EXES) $(TOOLS_EXES)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(EXEDIR):
	mkdir -p $(EXEDIR)

$(APPS_EXES): $(OBJECTS) $(APP_SOURCES) $(EXEDIR)
	$(CXX) $(CXXFLAGS) $(@:bin%=apps%.cpp) $(OBJECTS) $(EXT_OBJECTS) $(LIBS) -o $@ $(INCLUDE_DIRS)

$(TOOLS_EXES): $(OBJECTS) $(EXEDIR)
	$(CXX) $(CXXFLAGS) $(@:bin%=tools%.cpp) $(OBJECTS) $(EXT_OBJECTS) $(LIBS) -o $@ $(INCLUDE_DIRS)

$(TESTS_EXES): $(OBJECTS) $(EXEDIR)
	$(CXX) $(CXXFLAGS) $(@:bin%=test%.cpp) $(OBJECTS) ./lib/gtest-1.7.0/src/gtest_main.o ./lib/gtest-1.7.0/src/gtest-all.o $(EXT_OBJECTS) $(LIBS) -o $@ $(INCLUDE_DIRS) -DGOOGLE_TEST -I./lib/gtest-1.7.0/include/ -I./lib/gtest-1.7.0/ -I./apps/

$(OBJECTS): $(SOURCES) $(HEADERS) $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(LIB_INCS) -o $@ -c $(@:build%.o=src%.cpp) $(INCLUDE_DIRS)

gtest:
	cd ./lib/gtest-1.7.0/ && ./configure
	make -C ./lib/gtest-1.7.0

test: $(TESTS_EXES)
	@$(foreach bin, $(TESTS_EXES), $(bin);)

clean:
	rm -rf $(OBJDIR) $(EXEDIR)

