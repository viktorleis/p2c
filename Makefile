LIBS  = $(shell llvm-config --libs)
LIBS += -lclang-cpp
LIBS += -lz -lzstd

CXX = clang++

CXXFLAGS += -Wextra
CXXFLAGS += -Werror

query: p2c
	./p2c | clang-format --style=WebKit | tee gen.cpp && c++ -std=c++20 -Wall -O0 -g queryFrame.cpp -o query

p2c: p2c.cpp
	c++ -std=c++20 -Wall -O0 -g p2c.cpp -o p2c -lfmt $(LIBS) $(FLAGS)

clean:
	rm -f p2c query gen.cpp

.PHONY: clean
