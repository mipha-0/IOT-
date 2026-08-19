#
# Makefile
#-L/home/gec/freetype-2.12.1/tmp/lib -I/home/gec/freetype-2.12.1/tmp/include/freetype2
CC = arm-linux-gcc
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}
# 模块头文件搜索路径：各模块目录加入 -I 后，源码内 #include 可用短文件名
MOD_INC = -I$(LVGL_DIR)/src/app -I$(LVGL_DIR)/src/ui -I$(LVGL_DIR)/src/net \
          -I$(LVGL_DIR)/src/drivers -I$(LVGL_DIR)/src/data -I$(LVGL_DIR)/src/third_party
CFLAGS ?= -O3 -g0 -I$(LVGL_DIR)/ $(MOD_INC) -Wall -std=gnu99
LDFLAGS ?= -lm -lpthread
BIN = demo


#Collect the files to compile
MAINSRC = ./src/app/main.c
# 按模块分组的应用层源文件
FUNSRC = ./src/app/ui_manager.c \
         ./src/ui/ui_common.c ./src/ui/ui_navbar.c \
         ./src/ui/ui_page_home.c ./src/ui/ui_page_history.c \
         ./src/ui/ui_page_settings.c ./src/ui/ui_page_event.c \
         ./src/data/data_source.c \
         ./src/net/network.c \
         ./src/drivers/hw_ctrl.c \
         ./src/third_party/cJSON.c\



include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

CSRCS += $(LVGL_DIR)/mouse_cursor_icon.c



OBJEXT ?= .o

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))
FUNOBJ = $(FUNSRC:.c=$(OBJEXT))

MAINOBJ = $(MAINSRC:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS) $(MAINSRC)
OBJS = $(AOBJS) $(COBJS)

## MAINOBJ -> OBJFILES


all: default

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"

default: $(AOBJS) $(COBJS) $(MAINOBJ) $(FUNOBJ)
	$(CC) -o $(BIN) $(MAINOBJ) $(FUNOBJ) $(AOBJS) $(COBJS) $(LDFLAGS)

clean:
	rm -f $(BIN) $(AOBJS) $(COBJS) $(MAINOBJ) $(FUNOBJ)
