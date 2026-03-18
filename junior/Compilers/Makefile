CXX      := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic

TARGET := app
SRCS   := main.cpp lexer.cpp
OBJS   := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

main.o: main.cpp lexer.hpp parser.hpp ast.hpp analyzer.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lexer.o: lexer.cpp lexer.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
