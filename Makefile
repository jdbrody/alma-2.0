CC = gcc
CFLAGS = -std=c11 -pedantic-errors -Wall -Werror -Wshadow -Wpedantic -g

TOMMY = tommyds/tommyds/

all: alma.x

alma.x: alma.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o
	$(CC) alma.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o -o alma.x

alma.o: alma.c mpc/mpc.h alma_parser.h alma_formula.h alma_unify.h
	$(CC) $(CFLAGS) -c alma.c

tommyhashlin.o: $(TOMMY)tommyhash.c $(TOMMY)tommyhashlin.c $(TOMMY)tommytypes.h $(TOMMY)tommyhash.h $(TOMMY)tommylist.h $(TOMMY)tommyhashlin.h
	$(CC) $(CFLAGS) -c $(TOMMY)tommyhashlin.c

tommylist.o: $(TOMMY)tommylist.c $(TOMMY)tommytypes.h $(TOMMY)tommychain.h $(TOMMY)tommylist.h
	$(CC) $(CFLAGS) -c $(TOMMY)tommylist.c

tommyhash.o: $(TOMMY)tommyhash.c $(TOMMY)tommytypes.h $(TOMMY)tommyhash.h
	$(CC) $(CFLAGS) -c $(TOMMY)tommyhash.c

mpc.o: mpc/mpc.c mpc/mpc.h
	$(CC) $(CFLAGS) -c mpc/mpc.c

alma_parser.o: alma_parser.c mpc/mpc.h alma_parser.h
	$(CC) $(CFLAGS) -c alma_parser.c

alma_formula.o: alma_formula.c mpc/mpc.h alma_formula.h
	$(CC) $(CFLAGS) -c alma_formula.c

alma_kb.o: alma_kb.c alma_unify.h alma_formula.h alma_kb.h $(TOMMY)tommytypes.h $(TOMMY)tommyhashlin.h $(TOMMY)tommyhash.h
	$(CC) $(CFLAGS) -c alma_kb.c

alma_unify.o: alma_unify.c alma_unify.h alma_formula.h
	$(CC) $(CFLAGS) -c alma_unify.c

clean:
	rm -f *.x *.o

run:
	./alma.x formulae.pl
