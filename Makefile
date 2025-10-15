CXX ?= g++
FLAGS := -std=c++23 -g -Wall -O0 # -lfmt

# (1) run p2c
# (2) format the generated code if clang-format exists
# (3) compile generated code
query: p2c
	./p2c | (command -v clang-format >/dev/null 2>&1 && clang-format --style=WebKit || cat) > gen.cpp
	 c++ -std=c++23 -Wall -O0 -g queryFrame.cpp -o query
	$(CXX) $(FLAGS) -o query queryFrame.cpp 

# compile the query compiler p2c
p2c: p2c.cpp
	$(CXX) $(FLAGS) -o p2c p2c.cpp 

clean:
	rm -f p2c query gen.cpp

.PHONY: clean
