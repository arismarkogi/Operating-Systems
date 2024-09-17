TARGETS = child ask3

# Targets to build when running `make`
all: $(TARGETS)

# Phony targets. Executed always.
.PHONY: all clean

# Targets to build `child` binary
child: child.o
	gcc child.o -o child
child.o: child.c
	gcc child.c -c -o child.o

# Targets to build `gates` binary
ask3: parent.o
	gcc parent.o -o ask3
parent.o: parent.c
	gcc parent.c -c -o parent.o

# Run `make clean` to delete binaries and object files.
clean:
	rm -rf $(TARGETS) *.o

