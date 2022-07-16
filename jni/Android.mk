LOCAL_PATH := .

include $(CLEAR_VARS)
LOCAL_MODULE := libboron
LOCAL_CFLAGS := -DCONFIG_CHECKSUM -DCONFIG_COMPRESS=1 -DCONFIG_HASHMAP -DCONFIG_EXECUTE -DCONFIG_RANDOM -DCONFIG_SOCKET -DCONFIG_THREAD
LOCAL_C_INCLUDES := include urlan eval support
LOCAL_EXPORT_LDLIBS := -lz
LOCAL_SRC_FILES := urlan/hashmap.c \
    support/well512.c \
    eval/random.c \
    eval/port_socket.c \
    urlan/env.c \
    urlan/array.c \
    urlan/binary.c \
    urlan/block.c \
    urlan/coord.c \
    urlan/date.c \
    urlan/path.c \
    urlan/string.c \
    urlan/context.c \
    urlan/gc.c \
    urlan/serialize.c \
    urlan/tokenize.c \
    urlan/vector.c \
    urlan/parse_block.c \
    urlan/parse_string.c \
    support/str.c \
    support/mem_util.c \
    support/quickSortIndex.c \
    support/fpconv.c \
    eval/boron.c \
    eval/port_file.c \
    eval/port_thread.c \
    eval/wait.c \
    unix/os.c
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := boron2
LOCAL_C_INCLUDES := include support
LOCAL_SRC_FILES := eval/main.c
LOCAL_STATIC_LIBRARIES := boron
include $(BUILD_EXECUTABLE)
