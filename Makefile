VER=2.0.5

OS := $(shell uname)

CFLAGS = -pipe -pedantic -Wall -W -Iinclude -Iurlan -Ieval -Isupport
CFLAGS += -O3 -DNDEBUG
#CFLAGS = -g -DDEBUG

ifeq ($(OS), Darwin)
CFLAGS += -std=c99
AR_LIB = libtool -static -o
else
CFLAGS += -std=gnu99 -fPIC
AR_LIB = ar rc
endif

CONFIG := $(shell cat config.opt)

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

ifneq (,$(findstring _STATIC,$(CONFIG)))
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
$(ODIR):
	mkdir -p $@

$(BORON_LIB): $(LIB_OBJS)
ifneq (,$(findstring _STATIC,$(CONFIG)))
	$(AR_LIB) $@ $^
	ranlib $@
else ifeq ($(OS), Darwin)
	libtool -dynamiclib -o $@ $^ -install_name @rpath/$(BORON_LIB) $(LIBS)
else
	cc -o $@ -shared -Wl,-soname,libboron.so.2 $^ $(LIBS)
	ln -sf $(BORON_LIB) libboron.so.2
	ln -sf $(BORON_LIB) libboron.so
endif

.PHONY: clean
clean:
	rm -f boron $(BORON_LIB) $(LIB_OBJS) $(EXE_OBJS)
ifeq (,$(findstring _STATIC,$(CONFIG)))
	rm -f libboron.so*
endif
	rmdir $(ODIR)
