.PHONY: clean mrproper
.SUFFIXES:

SERVER = cadid
CLIENT = cadi
BINS = $(SERVER) $(CLIENT)

SERVER_OBJFILES = cadid.o process.o config.o
CLIENT_OBJFILES = cadi.o config.o
OBJFILES = $(SERVER_OBJFILES) $(CLIENT_OBJFILES)

CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -W -fno-builtin
LD = gcc
LDFLAGS = -s

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
	@echo [Clean] $(OBJFILES)

mrproper: clean
	@rm -f $(BINS)
	@echo [Clean] $(BINS)

