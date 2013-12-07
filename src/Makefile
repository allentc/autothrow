
SHELL = /bin/bash

progs = homograph canny

CXXFLAGS = $(shell pkg-config --cflags --libs ../opencv/lib/pkgconfig/opencv.pc)

all: $(progs)

clean:
	rm -fr $(progs)
