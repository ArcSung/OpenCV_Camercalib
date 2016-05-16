CXX ?= g++

CXXFLAGS += -c -std=c++11 -fexceptions -frtti -Wall $(shell pkg-config --cflags opencv)
LDFLAGS += $(shell pkg-config --libs --static opencv)

all: main

main: main.o; $(CXX) $< -o $@ $(LDFLAGS)

%.o: %.cpp; $(CXX) $< -o $@ $(CXXFLAGS)

clean: ; rm -f main.o main
