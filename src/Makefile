
SHELL = /bin/bash

#opencv = $(HOME)/apps/opencv-2.4.7
#opencvpc = $(opencv)/lib/pkgconfig/opencv.pc
#CXXFLAGS = -Wall -g -std=c++11 $(shell pkg-config --cflags $(opencvpc)) -Wl,-rpath=$(opencv)/lib
#LDLIBS = $(shell pkg-config --libs $(opencvpc))

CXXFLAGS = -Wall -g -std=c++11 $(shell pkg-config --cflags opencv)
LDLIBS = $(shell pkg-config --libs opencv)

progs = homograph canny play findContours_demo edges rotatedrect thinning black

all: $(progs)

clean:
	@rm -fr $(progs)

%: %.cpp
	@echo ' $(CXX)   '$<
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: all clean
