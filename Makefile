CC		   = gcc
CFLAGS	   = -I. -Wall -g -O0 -DUSE_PYTHON_MODULES
LDFLAGS	   = -ldl -g -O0 
OBJS	   =config.o \
		   curl_wrap.o \
		   irc.o \
		   module.o \
		   network.o \
		   py_wrap.o \
		   session.o \
		   socket_unix.o \
		   socket_win.o \
		   utils.o \
		   main.o

MOD_DIR    = ./modules

DEPS	   = python-2.7 libcurl

CFLAGS	   += $(shell pkg-config $(DEPS) --cflags)
LDFLAGS    += $(shell pkg-config $(DEPS) --libs)

CFLAGS     += -O0

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

all: neurobot modules

neurobot: $(OBJS)
	$(CC) $(OBJS) -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	rm -fv $(OBJS) neurobot; $(MAKE) --directory=$(MOD_DIR) clean

.PHONY: modules

modules:
	$(MAKE) --directory=$(MOD_DIR)

