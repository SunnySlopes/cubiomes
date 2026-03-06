CC       = gcc
ARFLAGS  = cr
override CFLAGS  += -Wall -Wextra -fwrapv -O2
override LDFLAGS += -lmicrohttpd -lpthread -lm

# cubiomes core source files (unchanged)
CUBIOMES_SRCS = noise.c biomes.c layers.c biomenoise.c \
                generator.c finders.c util.c quadbase.c
CUBIOMES_OBJS = $(CUBIOMES_SRCS:.c=.o)

# API server source files
SERVER_SRCS = src/main.c src/api.c src/engine.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

.PHONY: all server clean

all: server

server: $(SERVER_OBJS) libcubiomes.a
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) libcubiomes.a $(LDFLAGS)

libcubiomes.a: $(CUBIOMES_OBJS)
	ar $(ARFLAGS) $@ $^

# cubiomes core objects
noise.o: noise.c noise.h
	$(CC) -c $(CFLAGS) -o $@ $<

biomes.o: biomes.c biomes.h
	$(CC) -c $(CFLAGS) -o $@ $<

layers.o: layers.c layers.h
	$(CC) -c $(CFLAGS) -o $@ $<

biomenoise.o: biomenoise.c
	$(CC) -c $(CFLAGS) -o $@ $<

generator.o: generator.c generator.h
	$(CC) -c $(CFLAGS) -o $@ $<

finders.o: finders.c finders.h
	$(CC) -c $(CFLAGS) -o $@ $<

util.o: util.c util.h
	$(CC) -c $(CFLAGS) -o $@ $<

quadbase.o: quadbase.c quadbase.h
	$(CC) -c $(CFLAGS) -o $@ $<

# API server objects
src/main.o: src/main.c src/api.h
	$(CC) -c $(CFLAGS) -I. -o $@ $<

src/api.o: src/api.c src/api.h src/engine.h
	$(CC) -c $(CFLAGS) -I. -o $@ $<

src/engine.o: src/engine.c src/engine.h
	$(CC) -c $(CFLAGS) -I. -o $@ $<

clean:
	rm -f $(CUBIOMES_OBJS) $(SERVER_OBJS) libcubiomes.a server
