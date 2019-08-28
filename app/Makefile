TARGET = zkgui

#赋值对应的路径,此路径需要和
#PROJECT_PATH=/home/beal.wu/i2m_i6_master/project
PROJECT_PATH=/home/koda.xu/I6/master_i6_ipc/project

MINIGUI_PATH=./minigui_dir

COMPILE_ROOT := $(shell pwd)
CC = arm-linux-gnueabihf-g++
STRIP = arm-linux-gnueabihf-strip

CFLAGS = -I$(MINIGUI_PATH)/include -I$(MINIGUI_PATH)/include/freetype2 -I$(MINIGUI_PATH)/include/easyui -I$(PROJECT_PATH)/release/include
LDFLAGS = -L$(MINIGUI_PATH)/lib -L$(PROJECT_PATH)/release/nvr/i2m/010A/glibc/8.2.1/lib/dynamic/
CONFIG_PREFIX =.

LOCAL_CFLAGS =
LOCAL_LDFLAGS = -leasyui -lminigui_zk -lmgplus_zk -lpng12 -ljpeg -lpthread -lfreetype -ldl -lts -lz -lmi_common -lmi_gfx -lmi_sys -lmi_disp -lmi_hdmi -lmi_panel -lmi_vdec -lmi_ao -lmi_ai -lCSpotter -Wl,--hash-style=gnu

CPPSRCS += ${wildcard *.cpp}
CPPOBJS = $(patsubst %.cpp, %.cpp.o, $(CPPSRCS))

CSRCS += ${wildcard *.c}
COBJS = $(patsubst %.c,%.c.o,$(CSRCS))

all:$(TARGET) install

$(TARGET):$(CPPOBJS) $(COBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LOCAL_CFLAGS) $(LOCAL_LDFLAGS) -o $@ $^

$(CPPOBJS):%.cpp.o:%.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) $(LOCAL_CFLAGS) $(LOCAL_LDFLAGS) -c $^ -o $@

$(COBJS):%.c.o:%.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LOCAL_CFLAGS) $(LOCAL_LDFLAGS) -c $^ -o $@

install: 
	-mkdir -p $(CONFIG_PREFIX)/bin
	-mv $(TARGET) $(CONFIG_PREFIX)/bin/
	
clean:
	-rm -rf $(CPPOBJS) $(CONFIG_PREFIX)/bin/
	-rm -rf *.o
.PHONY:all clean $(TARGET) install
