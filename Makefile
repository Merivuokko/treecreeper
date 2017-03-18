DEBUG := 1

HOST_GXX := g++
CXXFLAGS := -std=gnu++14 -g2 -fPIC -fno-rtti -pipe -W -Wall -Wextra \
    -Wno-literal-suffix

ifeq "$(DEBUG)" "1"
    TARGET_GCC := gcc-svn
    CXXFLAGS += -DENABLE_TREE_CHECKING
else
    TARGET_GCC := gcc
endif

CXXINCLUDES := -I$(shell $(TARGET_GCC) -print-file-name=plugin)/include

srcdir := src
base_objdir := obj
objdir := $(base_objdir)/$(shell $(TARGET_GCC) -dumpmachine)/$(shell $(TARGET_GCC) -dumpversion)

sources := $(shell find $(srcdir) -type f -name '*.cc')
objects := $(patsubst $(srcdir)/%,$(objdir)/%,$(sources:.cc=.o))

plugin := $(objdir)/treecreeper.so

all: $(plugin) $(objects)
.PHONY: all

show:
	@echo Sources: $(sources)
	@echo Objects: $(objects)
.PHONY: show

$(objdir):
	mkdir -p $(objdir)

-include $(objects:.o=.dep)

$(objdir)/%.o: $(srcdir)/%.cc | $(objdir)
	$(HOST_GXX) $(CXXFLAGS) $(CXXINCLUDES) -c \
	    -MMD -MP -MF $(objdir)/$*.dep \
	    $(srcdir)/$*.cc -o $(objdir)/$*.o

$(plugin): $(objects) | $(objdir)
	$(HOST_GXX) -shared -rdynamic -o $@ $(objects)

run:
	$(TARGET_GCC) -x c++ -S -std=gnu++14 -fplugin=./$(plugin) \
	    -fplugin-arg-treecreeper-output=test.cc.json test.cc

run-c:
	$(TARGET_GCC) -x c -S -fplugin=./$(plugin) \
	    -fplugin-arg-treecreeper-output=test.c.json test.c

run-c++:
	$(TARGET_GCC) -x c++ -S -fplugin=./$(plugin) \
	    -fplugin-arg-treecreeper-output=test.c.json test.c

run-valgrind-c:
	valgrind --trace-children=yes -- \
	    $(TARGET_GCC) -x c -S -fplugin=./$(plugin) \
	        -fplugin-arg-treecreeper-output=test.c.json test.c

run-valgrind:
	valgrind --trace-children=yes -- \
	    $(TARGET_GCC) -S -std=gnu++14 -fplugin=./$(plugin) \
	        -fplugin-arg-treecreeper-output=test.cc.json test.cc

clean:
	rm -rf $(objdir)
.PHONY: clean

distclean:
	rm -rf $(base_objdir)
.PHONY: distclean
