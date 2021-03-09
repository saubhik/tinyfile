SRCDIR := src
INCDIR := include
OBJDIR := obj
BINDIR := bin

CC := gcc
CCFLAGS  := -Wall -c -DDEBUG=0 -O3
INCLUDES := -I$(INCDIR)
LIBS := -lrt -lpthread

CLIENT := $(BINDIR)/tinyfile-client
SERVER := $(BINDIR)/tinyfile-server
LIB := $(BINDIR)/libtinyfile.a

.PHONY: all lib client server clean

all: lib client server

client: $(CLIENT)

server: $(SERVER)

lib: $(LIB)

$(LIB): $(OBJDIR)/tinyfile_api.o | $(BINDIR)
	ar rcs $(LIB) $(OBJDIR)/tinyfile_api.o

$(CLIENT): $(OBJDIR)/client.o | $(BINDIR)
	$(CC) -L$(BINDIR) $^ -o $@ -ltinyfile $(LIBS)

$(SERVER): $(OBJDIR)/tinyfile_server.o | $(BINDIR)
	$(CC) include/snappy-c/snappy.c $^ -o $@ $(LIBS)

$(OBJDIR)/client.o: $(LIB) client.c | $(OBJDIR)
	$(CC) $(CCFLAGS) $(INCLUDES) client.c -o $@

$(OBJDIR)/tinyfile_api.o: $(SRCDIR)/api/api.c $(INCDIR)/tinyfile/api.h | $(OBJDIR)
	$(CC) $(CCFLAGS) $(INCLUDES) $(SRCDIR)/api/api.c -o $@ $(LIBS)

$(OBJDIR)/tinyfile_server.o: $(SRCDIR)/server/server.c $(INCDIR)/tinyfile/server.h | $(OBJDIR)
	$(CC) $(CCFLAGS) $(INCLUDES) $(SRCDIR)/server/server.c -o $@

$(BINDIR):
	mkdir -p $@

$(OBJDIR):
	mkdir -p $@

clean:
	rm -f $(OBJDIR)/*.o
	rm $(BINDIR)/* || true
