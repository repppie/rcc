PROG = rcc

SRCS = rcc.c lex.yy.c parse.c ir.c x86.c sym.c cfg.c lib.c
HEADERS = rcc.h
OBJS = $(SRCS:.c=.o)

cc = gcc
CFLAGS = -Wall -g
#LDFLAGS += -ll

all: $(PROG)

clean:
	rm -f $(OBJS) $(PROG) lex.yy.c

lex.yy.o: lex.yy.c
lex.yy.c: lex.l
	lex lex.l

$(PROG): $(OBJS) $(HEADERS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
