query: p2c
	./p2c | clang-format --style=WebKit | tee gen.cpp && c++ -std=c++23 -Wall -O0 -g queryFrame.cpp -o query

p2c: p2c.cpp
	c++ -std=c++23 -Wall -O0 -g p2c.cpp -o p2c

clean:
	rm -f p2c query gen.cpp

.PHONY: clean
