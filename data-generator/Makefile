##
# csv-read
#
# @file
# @version 0.1
COMPILER = g++
FLAGS = -Wall -std=c++23 -pthread -mavx2 
DEBUG_FLAGS = -g -fno-omit-frame-pointer -fsanitize=address -O0
RELEASE_FLAGS = -O3

all: all.out

%.out: clean
ifeq ($(target),debug)
	${COMPILER} $(patsubst %.out, %.cpp, $@) ${FLAGS} ${DEBUG_FLAGS} -o $@
else
	${COMPILER} $(patsubst %.out, %.cpp, $@) ${FLAGS} ${RELEASE_FLAGS} -o $@
endif


clean:
	rm -f *.out

.PHONY: clean all

# end
