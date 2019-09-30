LOCAL_PATH := .

USR_LIB=$(ANDROID_HOME)/local/$(TARGET_ARCH_ABI)

include $(CLEAR_VARS)
LOCAL_MODULE := freetype
LOCAL_SRC_FILES := $(USR_LIB)/lib/libfreetype.a
LOCAL_EXPORT_C_INCLUDES := $(USR_LIB)/include/freetype2
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := png
LOCAL_SRC_FILES := $(USR_LIB)/lib/libpng.a
LOCAL_EXPORT_C_INCLUDES := $(USR_LIB)/include
include $(PREBUILT_STATIC_LIBRARY)


# include $(CLEAR_VARS)
# LOCAL_MODULE := glv
# LOCAL_SRC_FILES := $(USR_LIB)/lib/libglv.a
# LOCAL_EXPORT_C_INCLUDES := $(USR_LIB)/include
# include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := boron
LOCAL_SRC_FILES := ../obj/local/$(TARGET_ARCH_ABI)/libboron.a
LOCAL_EXPORT_C_INCLUDES := ../include
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libboron-gl
LOCAL_CFLAGS := -DNO_AUDIO
LOCAL_C_INCLUDES := ../include ../urlan jni/glv
LOCAL_SRC_FILES := es_compat.c \
    audio_stub.c \
    boron-gl.c \
    draw_prog.c \
    geo.c \
    glid.c \
    gui.c \
    png_load.c \
    png_save.c \
    quat.c \
    raster.c \
    rfont.c \
    shader.c \
    TexFont.c \
    port_joystick.c \
    widget_button.c \
    widget_label.c \
    widget_lineedit.c \
    widget_list.c \
    widget_slider.c \
    widget_itemview.c
# LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv2 -lz
LOCAL_STATIC_LIBRARIES := freetype png  #openal vorbis vorbisfile glv
include $(BUILD_STATIC_LIBRARY)
