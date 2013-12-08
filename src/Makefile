
SHELL = /bin/bash

opencv = $(HOME)/apps/opencv-2.4.7
opencvpc = $(opencv)/lib/pkgconfig/opencv.pc

CXXFLAGS = $(shell pkg-config --cflags $(opencvpc)) -Wl,-rpath=$(opencv)/lib
LDLIBS = $(shell pkg-config --libs $(opencvpc))

progs = homograph canny

all: $(progs)

clean:
	rm -fr $(progs)

.PHONY: all clean
