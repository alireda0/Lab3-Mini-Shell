CXX      = g++
CXXFLAGS = -Wall -Wextra -g -std=c++17
OBJS     = command.o tokenizer.o

myshell: $(OBJS)
	$(CXX) $(CXXFLAGS) -o myshell $(OBJS)

command.o: command.cc command.h tokenizer.h
	$(CXX) $(CXXFLAGS) -c command.cc

tokenizer.o: tokenizer.cc tokenizer.h
	$(CXX) $(CXXFLAGS) -c tokenizer.cc

clean:
	rm -f *.o myshell

.PHONY: clean
