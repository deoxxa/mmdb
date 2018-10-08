default: mmdb

CFLAGS+=-Werror
LDLIBS=-lsqlite3 -lyder -ljansson -lcrypto

mmdb: main.c mmdb.o q.o

mmdb_tests: mmdb_tests.c mmdb_tests_*.c munit/munit.o mmdb.o q.o

.PHONY: test
test: mmdb_tests
	./mmdb_tests

.PHONY: watch
watch:
	sh watch.sh

.PHONY: clean
clean:
	rm -f mmdb mmdb_tests *.o
