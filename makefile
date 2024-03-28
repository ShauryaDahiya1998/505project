TARGETS = frontserver

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

frontserver: frontserver.o
	g++ $^ -lpthread -o $@
