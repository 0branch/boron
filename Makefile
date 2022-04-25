# Boron Makefile for UNIX systems.

VER=2.0.8

DESTDIR ?= /usr/local
BIN_DIR=$(DESTDIR)/bin
LIB_DIR=$(DESTDIR)/lib
INC_DIR=$(DESTDIR)/include/boron
MAN_DIR=$(DESTDIR)/share/man/man1
VIM_DIR=$(DESTDIR)/share/vim/vimfiles/syntax

OS := $(shell uname)

CFLAGS = -pipe -pedantic -Wall -W -Iinclude -Iurlan -Ieval -Isupport
CFLAGS += -O3 -DNDEBUG
#CFLAGS += -g -DDEBUG

ifeq ($(OS), Darwin)
CFLAGS += -std=c99
AR_LIB = libtool -static -o
else
CFLAGS += -std=gnu99 -fPIC
AR_LIB = ar rc
ifneq (,$(wildcard /usr/lib64/libc.so))
LIB_DIR=$(DESTDIR)/lib64
else ifneq (,$(wildcard /usr/lib/x86_64-linux-gnu/.))
LIB_DIR=$(DESTDIR)/lib/x86_64-linux-gnu
endif
endif

CONFIG := $(shell cat config.opt)
ifneq (,$(findstring _STATIC,$(CONFIG)))
	STATIC_LIB = true
endif

LIBS := -lm

ODIR = .obj
OBJ_FN = env.o array.o binary.o block.o coord.o date.o path.o \
	string.o context.o gc.o serialize.o tokenize.o \
	vector.o parse_block.o parse_string.o
OBJ_FN += str.o mem_util.o quickSortIndex.o fpconv.o
OBJ_FN += os.o boron.o port_file.o wait.o
ifneq (,$(findstring _HASHMAP,$(CONFIG)))
OBJ_FN += hashmap.o
endif
ifneq (,$(findstring _RANDOM,$(CONFIG)))
OBJ_FN += well512.o random.o
endif
ifneq (,$(findstring _SOCKET,$(CONFIG)))
OBJ_FN += port_socket.o
endif
ifneq (,$(findstring _THREAD,$(CONFIG)))
OBJ_FN += port_thread.o
ifeq ($(OS), Linux)
LIBS += -lpthread
endif
endif
LIB_OBJS = $(addprefix $(ODIR)/,$(OBJ_FN))


MAIN_FN = main.o
ifneq (,$(findstring _LINENOISE,$(CONFIG)))
MAIN_FN += linenoise.o
else
EXE_LIBS += -lreadline -lhistory
endif
ifneq (,$(findstring _COMPRESS=1,$(CONFIG)))
LIBS += -lz
endif
ifneq (,$(findstring _COMPRESS=2,$(CONFIG)))
LIBS += -lbz2
endif
EXE_OBJS = $(addprefix $(ODIR)/,$(MAIN_FN))

ifdef STATIC_LIB
BORON_LIB = libboron.a
EXE_LIBS += $(LIBS)
else ifeq ($(OS), Darwin)
BORON_LIB = libboron.dylib
else
BORON_LIB = libboron.so.$(VER)
endif


$(ODIR)/%.o: urlan/%.c
	cc -c $(CFLAGS) $(CONFIG) $< -o $@
$(ODIR)/%.o: support/%.c
	cc -c $(CFLAGS) $(CONFIG) $< -o $@
$(ODIR)/%.o: eval/%.c
	cc -c $(CFLAGS) $(CONFIG) $< -o $@

boron: $(EXE_OBJS) $(BORON_LIB)
	cc $^ -o $@ $(EXE_LIBS)

$(ODIR)/os.o: unix/os.c
	cc -c $(CFLAGS) $(CONFIG) $< -o $@

$(EXE_OBJS): | $(ODIR)
$(LIB_OBJS): | $(ODIR)
$(ODIR):
	mkdir -p $@

$(BORON_LIB): $(LIB_OBJS)
ifdef STATIC_LIB
	$(AR_LIB) $@ $^
	ranlib $@
else ifeq ($(OS), Darwin)
	libtool -dynamiclib -o $@ $^ -install_name @rpath/$(BORON_LIB) $(LIBS)
else
	cc -o $@ -shared -Wl,-soname,libboron.so.2 $^ $(LIBS)
	ln -sf $(BORON_LIB) libboron.so.2
	ln -sf $(BORON_LIB) libboron.so
endif

.PHONY: clean install uninstall install-dev uninstall-dev

clean:
	rm -f boron $(BORON_LIB) $(LIB_OBJS) $(EXE_OBJS)
ifndef STATIC_LIB
	rm -f libboron.so*
endif
	rmdir $(ODIR)

install:
	mkdir -p $(BIN_DIR) $(LIB_DIR) $(MAN_DIR)
ifndef STATIC_LIB
ifeq ($(OS), Darwin)
	install_name_tool -id $(LIB_DIR)/libboron.dylib libboron.dylib
	install_name_tool -change libboron.dylib $(LIB_DIR)/libboron.dylib boron
	install -m 644 libboron.dylib $(LIB_DIR)
else
	install -m 755 -s $(BORON_LIB) $(LIB_DIR)
	ln -s $(BORON_LIB) $(LIB_DIR)/libboron.so.2
endif
endif
	install -s -m 755 boron $(BIN_DIR)
	gzip -c -n doc/boron.troff > doc/boron.1.gz
	install -m 644 doc/boron.1.gz $(MAN_DIR)

uninstall:
	rm -f $(BIN_DIR)/boron $(MAN_DIR)/boron.1
ifndef STATIC_LIB
	rm -f $(LIB_DIR)/$(BORON_LIB)
ifneq ($(OS), Darwin)
	rm -f $(LIB_DIR)/libboron.so.2
endif
endif

install-dev:
	mkdir -p $(INC_DIR) $(LIB_DIR)
	sed -e 's~"urlan.h"~<boron/urlan.h>~' include/boron.h >boron.tmp
	install -m 644 boron.tmp $(INC_DIR)/boron.h
	rm boron.tmp
	install -m 644 include/urlan.h        $(INC_DIR)
	install -m 644 include/urlan_atoms.h  $(INC_DIR)
#	install -m 755 scripts/copr.b $(BIN_DIR)/copr
ifdef STATIC_LIB
	install -m 644 $(BORON_LIB) $(LIB_DIR)
endif
ifneq ($(OS), Darwin)
	mkdir -p $(VIM_DIR)
	install -m 644 doc/boron.vim $(VIM_DIR)
ifndef STATIC_LIB
	ln -s $(BORON_LIB) $(LIB_DIR)/libboron.so
endif
endif

uninstall-dev:
ifdef STATIC_LIB
	rm -f $(LIB_DIR)/$(BORON_LIB)
endif
ifneq ($(OS), Darwin)
ifndef STATIC_LIB
	rm -f $(LIB_DIR)/libboron.so
endif
	rm -f $(VIM_DIR)/boron.vim
endif
#	rm -f $(BIN_DIR)/copr
	rm -f $(INC_DIR)/boron.h $(INC_DIR)/urlan.h $(INC_DIR)/urlan_atoms.h
	rmdir $(INC_DIR)
