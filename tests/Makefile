all: tests
	LD_LIBRARY_PATH=../lib/ ./tests

tests: tests.o
	$(CC) -o $@ $< -L ../lib -lvconf

tests.o: tests.c
	$(CC) -c -o $@ $< -I ../include -g

clean:
	-rm tests.[od] tests

-include tests.d