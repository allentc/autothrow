
SHELL = /bin/bash

opencv = $(HOME)/apps/opencv-2.4.7
opencvpc = $(opencv)/lib/pkgconfig/opencv.pc

CXXFLAGS = -Wall -g $(shell pkg-config --cflags $(opencvpc)) -Wl,-rpath=$(opencv)/lib
LDLIBS = $(shell pkg-config --libs $(opencvpc))

progs = homograph canny play

all: $(progs)

clean:
	@rm -fr $(progs)

%: %.cpp
	@echo ' $(CXX)   '$<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: all clean
