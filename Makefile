CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -ansi -Wno-unused-parameter
TARGET  = adventure
SRCDIR  = src
INCDIR  = include
OBJDIR  = obj

SRCS    = $(wildcard $(SRCDIR)/*.c)
OBJS    = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(OBJDIR) $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

HDRS    = $(wildcard $(INCDIR)/*.h)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
