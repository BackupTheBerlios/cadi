.PHONY: clean
.SUFFIXES:


SERVER = cadid
CLIENT = cadi
BINS = $(SERVER) $(CLIENT)

SERVER_OBJFILES = cadid.o
CLIENT_OBJFILES = cadi.o
OBJFILES = $(SERVER_OBJFILES) $(CLIENT_OBJFILES)

CC = gcc
CFLAGS = -ansi -pedantic -Wall -W -Werror -fno-builtin
LD = gcc
LDFLAGS = -s -lpthread

all: $(BINS)

$(SERVER): $(SERVER_OBJFILES)
	@$(LD) $(LDFLAGS) -o $@ $^
	@echo [L] $@

$(CLIENT): $(CLIENT_OBJFILES)
	@$(LD) $(LDFLAGS) -o $@ $^
	@echo [L] $@

%.o: %.c
	@$(CC) $(CFLAGS) -c $<
	@echo [C] $@

clean:
	@rm -f $(OBJFILES)
	@rm -f $(BINS)
	@echo [Clean] $(OBJFILES) $(BINS)
