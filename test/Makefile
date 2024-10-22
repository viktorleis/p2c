LIBS  = $(shell llvm-config --libs)
LIBS += -lclang-cpp

CXX = clang++

CXXFLAGS  = -Wall
CXXFLAGS += -Wextra
CXXFLAGS += -Werror
CXXFLAGS += -O3

run: gen-ir
	./$^

gen-ir: gen-ir.o
	$(CXX) -o $@ $^ $(LIBS) $(FLAGS)

gen-ir.o: gen-ir.cc
	$(CXX) -o $@ -c $^ $(CXXFLAGS) $(FLAGS)

fmt:
	clang-format -i *.cc

clean:
	$(RM) gen-ir *.o

