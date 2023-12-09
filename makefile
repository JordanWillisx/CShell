CC = gcc
CFLAGS = -g -Wall -std=gnu17 -Iinclude
LDLIBS = -lrt
LFLAGS = 
SRCDIR = src
OBJDIR = obj
BUILDDIR = build

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
EXECUTABLE = $(BUILDDIR)/major2.exe

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS) $(LDLIBS) -pthread

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(OBJDIR) $(BUILDDIR)

run: $(EXECUTABLE)
	./$(EXECUTABLE)

debug: $(EXECUTABLE)
	valgrind $(EXECUTABLE)

valgrind: $(EXECUTABLE)
	valgrind $(EXECUTABLE)

valgrind_leakcheck: $(EXECUTABLE)
	valgrind --leak-check=full $(EXECUTABLE)

valgrind_extreme: $(EXECUTABLE)
	valgrind --leak-check=full --show-leak-kinds=all --leak-resolution=high --track-origins=yes --vgdb=yes $(EXECUTABLE)
