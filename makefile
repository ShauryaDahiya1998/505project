TARGETS = frontserver frontserver1

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

frontserver: frontserver.o
	g++ $^ -lpthread -o $@

frontserver1: frontserver1.o
	g++ $^ -lpthread -o $@
