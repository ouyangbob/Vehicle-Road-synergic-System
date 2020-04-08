# CROSS_COMPILE = /opt/fsl-framebuffer/2.4.4/sysroots/cortexa9hf-neon-

# CC		= $(CROSS_COMPILE)fsl-linux-gnueabi
 
# AS      = as
# LD      = ld
#CC      = gcc
# CPP     = $(CC) -E
# AR      = ar
# NM      = nm
 
# 程序的默认名称
#TARGET = roadblock
TARGET = speedlimit

# 命令行中使用p=X或P=X修改默认程序名称
ifeq ("$(origin p)", "command line")
TARGET = $(p)
endif
 
ifeq ("$(origin P)", "command line")
TARGET = $(P)
endif
 
DIRS = $(shell find . -type d)
 
# 找出所有的.c .h .a .so文件及目录
CFILES_DIR 	= $(shell find . -type f -name "*.c")
CFILES 		= $(notdir $(CFILES_DIR))
 
HFILES_DIR 	= $(shell find . -type f -name "*.h")
HFILES 		= $(notdir $(HFILES_DIR))
HDIRS		= $(sort $(dir $(HFILES_DIR)))
 
# AFILES_DIR 	= $(shell find . -type f -name "lib*.a")
# AFILES		= $(notdir $(AFILES_DIR))
# ADIRS		= $(sort $(dir $(AFILES_DIR)))
 
# SOFILES_DIR = $(shell find . -type f -name "lib*.so")
# SOFILES		= $(notdir $(SOFILES_DIR))
# SODIRS		= $(sort $(dir $(SOFILES_DIR)))
 
# 包含所有含有.h文件的目录
INCLUDES += $(HDIRS:%=-I%)

$(warning INCLUDES : [ $(INCLUDES) ])
 
CFLAGS 	+= $(INCLUDES)
 
# 编译标志
# CFLAGS 	+= -Wall -Werror
# CFLAGS 	+= -g -MD -O2 -static
 
# 含有.a .so文件的目录
# LDFLAGS += $(ADIRS:%=-L%)
# LDFLAGS += $(SODIRS:%=-L%)
 
# 引用库文件
# LDFLAGS += $(AFILES:lib%.a=-l%)
# LDFLAGS += $(SOFILES:lib%.so=-l%)
 
LDFLAGS := -lm -lrt -lpthread -lglib-2.0 -lv2x_radio -lv2x_radio -lcv2x-kinematics
# LDFLAGS += -lpthread
 
$(warning LDFLAGS : [ $(LDFLAGS) ])
 
# 包含所有的目录
VPATH = $(DIRS)
 
# 所有的.c文件
SOURCES = $(CFILES)
 
$(warning SOURCES : [ $(SOURCES) ])
 
# 目标及依赖
OBJS	= $(SOURCES:%.c=obj/%.o)
DEPS 	= $(SOURCES:%.c=obj/%.d)
 
###########################################################
 
.PHONY : all clean cleanall
 
all : $(TARGET)
 
$(TARGET) : $(OBJS)
	@echo
	@echo "Linking..."
	@echo 
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
	@echo
	@echo "enjoy < $(TARGET) > Good Luck."
	@echo
 
obj/%.o : %.c
	@mkdir -p obj 
	@echo $(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@
 
clean :
	rm -rf $(OBJS) $(DEPS) obj
 
cleanall :
	rm -rf $(OBJS) $(DEPS) obj $(TARGET)
 
# 包含生成的依赖关系
sinclude $(DEPS)


