INCLUDE_DIR := .

query: p2c
	./p2c | clang-format --style=WebKit | tee gen.cpp && c++ -std=c++20 -Wall -O0 -g queryFrame.cpp -o query

p2c: p2c.cpp operators/*
	c++ -std=c++20 -Wall -O0 -g p2c.cpp -o p2c -lfmt -I$(INCLUDE_DIR)

clean:
	rm -f p2c query gen.cpp

.PHONY: clean
