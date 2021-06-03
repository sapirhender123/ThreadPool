all: test clean

debug: CCFLAGS += -DDEBUG -g
debug: test clean

test: threadPool.o
	gcc $(CCFLAGS) test.c threadPool.o osqueue.o -lpthread -Wall -Werror

threadPool.o: osqueue.o
	gcc $(CCFLAGS) -c threadPool.c

osqueue.o:
	gcc $(CCFLAGS) -c osqueue.c

clean:
	rm -f *.o