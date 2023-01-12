query: p2c
	./p2c | clang-format --style=WebKit | tee gen.cpp && clang++ -std=c++20 -Wall -O0 -g queryFrame.cpp -o query

p2c: p2c.cpp
	clang++ -std=c++20 -Wall -O0 -g p2c.cpp -o p2c -lfmt

clean:
	rm -f p2c query gen.cpp

.PHONY: clean
