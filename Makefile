SRCS:=$(wildcard src/*.cpp)
SRCS:=$(filter-out src/main.cpp, $(SRCS))
SRCS:=$(filter-out src/main-demote.cpp, $(SRCS))
SRCS:=$(filter-out src/main-promote.cpp, $(SRCS))
OBJS :=   $(SRCS:%.cpp=%.o)

ERR = $(shell which icpc >/dev/null; echo $$?)
ifeq "$(ERR)" "0"
    CXX = icpc
else
    CXX = g++
endif

# Compile in Debug mode :
# CPPFLAG = -g -DDEBUG -DVERB -std=c++0x -Wall
CPPFLAG = -g -DDEBUG -DVERB -std=c++0x -Wall -DREQSIZE -lbf
# CPPFLAG = -g -DDEBUG  -std=c++0x -Wall

# Compile in Resealese mode:
# CPPFLAG = -O3 -DNDEBUG -std=c++0x -Wall
# CPPFLAG = -O3 -DNDEBUG -std=c++0x -Wall -DREQSIZE
# CPPFLAG = -O3 -DNDEBUG -std=c++0x -Wall -DHIST -DREQSIZE


all: main demote promote

main: $(OBJS) src/main.o
	$(CXX) -o sim-ideal-main  $(OBJS) src/main.o $(LDFLAG) $(CPPFLAG)

demote: $(OBJS) src/main-demote.o
	$(CXX) -o sim-ideal-demote  $(OBJS) src/main-demote.o $(LDFLAG) $(CPPFLAG)
	
promote: $(OBJS) src/main-promote.o
	$(CXX) -o sim-ideal-promote  $(OBJS) src/main-promote.o $(LDFLAG) $(CPPFLAG)

%.o: %.cpp
	$(CXX) $(CPPFLAG) -c $< -o $@

main.o: 
	$(CXX) $(CPPFLAG) -c src/main.cpp -o src/$@
	
main-demote.o: 
	$(CXX) $(CPPFLAG) -c src/main-demote.cpp -o src/$@

main-promote.o: 
	$(CXX) $(CPPFLAG) -c src/main-promote.cpp -o src/$@
	
clean:
	rm -f src/*.o
	rm -f sim-ideal
