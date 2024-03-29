TARGETS = frontserver frontserverV1

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

frontserver: frontserver.o
	g++ $^ -lpthread -o $@

frontserverV1: frontserverV1.o
	g++ $^ -lpthread -o $@
