# (1) run p2c
# (2) format the generated code if clang-format exists
# (3) compile generated code
query: p2c
	./p2c | (command -v clang-format >/dev/null 2>&1 && clang-format --style=WebKit || cat) > gen.cpp
	 c++ -std=c++23 -Wall -O0 -g queryFrame.cpp -o query

# compile the query compiler p2c
p2c: p2c.cpp
	c++ -std=c++23 -Wall -O0 -g p2c.cpp -o p2c

clean:
	rm -f p2c query gen.cpp

.PHONY: clean
