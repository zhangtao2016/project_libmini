COLOR_NONE            = "\033[m"
COLOR_GRAY            = "\033[1;30m"
COLOR_RED             = "\033[1;31m"
COLOR_GREEN           = "\033[1;32m"
COLOR_YELLOW          = "\033[1;33m"
COLOR_BLUE            = "\033[1;34m"
COLOR_PURPLE          = "\033[1;35m"
COLOR_CYAN            = "\033[1;36m"
COLOR_WHITE           = "\033[1;37m"

#######

COLOR_OBJ =		$(COLOR_CYAN)
COLOR_TAG =		$(COLOR_YELLOW)
COLOR_TXT = 		$(COLOR_GREEN)
#######
BUILD_PWD = 		$(shell pwd)
BUILD_HOST =		$(shell uname | tr '[A-Z]' '[a-z]')

TOBJ =			libmini
TOBJ_MAJOR ?= 		1
TOBJ_MINOR ?= 		0

TOBJ_ARNAME =		$(TOBJ).a

SOBJ_DIR =		../../.objs
TOBJ_DIR =		../../lib

#######

BUILD_CFLAGS =		-fPIC -Wall -Wno-unused-function -Wno-unused-variable
BUILD_LIBS =		-lpthread -lm
BUILD_INC_DIR = 	-I/usr/include -I/usr/local/include -I../../include/libmini
BUILD_LIB_DIR =		-L/usr/local/lib -L/usr/lib -L/usr/libexec

#######

ifeq ($(BUILD_HOST), darwin)

ECHO =			@echo
TOBJ_DLSUFFIX =		dylib
TOBJ_NAME = 		$(TOBJ).$(TOBJ_DLSUFFIX)
TOBJ_DLNAME =		$(TOBJ_NAME)
DYLIB_CFLAGS =		-dynamiclib
else
BUILD_CFLAGS += 	-D_GNU_SOURCE -export-dynamic
BUILD_LIB_DIR +=	-L/usr/local/lib64 -L/usr/lib64

ECHO =			@echo -e
TOBJ_DLSUFFIX =		so
TOBJ_NAME = 		$(TOBJ).$(TOBJ_DLSUFFIX)
TOBJ_DLNAME =		$(TOBJ_NAME).$(TOBJ_MAJOR).$(TOBJ_MINOR)
TOBJ_SONAME = 		$(TOBJ_NAME).$(TOBJ_MAJOR)
DYLIB_CFLAGS =		-shared -Wl,-soname,$(TOBJ_SONAME)
#BUILD_OPTIMIZE =	-fprofile-arcs
BUILD_LIBS =		-lrt
endif



#######

CC_COMPILER ?=		@gcc
CC =			$(CC_COMPILER) $(BUILD_CFLAGS) $(CFLAGS) $(BUILD_INC_DIR) $(BUILD_OPTIMIZE)
LN = 			$(CC_COMPILER) $(BUILD_CFLAGS) $(CFLAGS) $(DYLIB_CFLAGS) $(BUILD_LIB_DIR)
AR = 			@ar -rcs

#######

define compile_obj
	$(ECHO) $(COLOR_TXT)"\t- COMPILE\t===>\t"$(COLOR_OBJ)"$(1:.c=.o)"$(COLOR_NONE)
	$(CC) -c $(1) -o $(2)
endef

all: prepare $(TOBJ)

prepare:
	@-if [ ! -d $(SOBJ_DIR) ];then mkdir $(SOBJ_DIR); fi
	@-if [ ! -d $(TOBJ_DIR) ];then mkdir $(TOBJ_DIR); fi

#######以下是你需要修改添加的#######

SFILE_BASE_DIR = 	src

SFILE_ATOMIC_DIR = 	$(SFILE_BASE_DIR)/atomic
SFILE_ADT_DIR =         $(SFILE_BASE_DIR)/adt
SFILE_ADT_STATIC_DIR =  $(SFILE_BASE_DIR)/adt/static
SFILE_ADT_DYNAMIC_DIR = $(SFILE_BASE_DIR)/adt/dynamic
SFILE_STRING_DIR =  	$(SFILE_BASE_DIR)/string
SFILE_TIME_DIR =    	$(SFILE_BASE_DIR)/time
SFILE_SLOG_DIR =    	$(SFILE_BASE_DIR)/slog
SFILE_IO_DIR =    	$(SFILE_BASE_DIR)/io
SFILE_IPC_DIR =    	$(SFILE_BASE_DIR)/ipc
SFILE_THREAD_DIR =    	$(SFILE_BASE_DIR)/thread
SFILE_EXCEPT_DIR = 	$(SFILE_BASE_DIR)/except
SFILE_SOCKET_DIR = 	$(SFILE_BASE_DIR)/socket
SFILE_FILESYSTEM_DIR = 	$(SFILE_BASE_DIR)/filesystem

#######
SOBJ = $(addprefix $(SOBJ_DIR)/, \
	utils.o \
	atomic.o \
	slog.o \
	adt_proto.o \
	squeue.o \
	slist.o \
	shmqueue.o \
	filequeue.o \
	memqueue.o \
	dstack.o \
	dqueue.o \
	dhash.o \
	string.o \
	time.o \
	io_base.o \
	pipe.o \
	fork.o \
	futex.o \
	signal.o \
	ipc_base.o \
	thread.o \
	except.o \
	exception.o \
	socket_base.o \
	socket_opt.o \
	socket.o \
	filesystem.o \
	filelock.o \
	)

#######

$(SOBJ_DIR)/%.o : $(SFILE_BASE_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_ATOMIC_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_ADT_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_ADT_STATIC_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_ADT_DYNAMIC_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_STRING_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_TIME_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_SLOG_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_IO_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_IPC_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_THREAD_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_EXCEPT_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_SOCKET_DIR)/%.c
	$(call compile_obj, $<, $@)

$(SOBJ_DIR)/%.o : $(SFILE_FILESYSTEM_DIR)/%.c
	$(call compile_obj, $<, $@)

##############
#拷贝头文件
INSTALL_HEAD_DIR = ../../include
INSTALL_HEAD_FILE = 	@-rm -rf $(INSTALL_HEAD_DIR); \
			cp -rf $(SFILE_BASE_DIR) $(INSTALL_HEAD_DIR);	\
			find $(INSTALL_HEAD_DIR) -name "*.c" -exec rm {} \;
#创建软链接
ifeq ($(BUILD_HOST), linux)
INSTALL_DYLIB_FILE = 	@-ln -sf $(TOBJ_DLNAME) $(TOBJ_DIR)/$(TOBJ_SONAME); \
			ln -sf $(TOBJ_SONAME) $(TOBJ_DIR)/$(TOBJ_NAME)
else
INSTALL_DYLIB_FILE =	@echo $(COLOR_RED)"Not support multi-version of library"$(COLOR_NONE)
endif
#######以上是你需要修改添加的#######


$(TOBJ) : $(SOBJ)
#$(INSTALL_HEAD_FILE)
	$(ECHO) $(COLOR_TXT)"\t\t- ARCHIVE\t===>\t"$(COLOR_TAG)"$(TOBJ_ARNAME)"$(COLOR_NONE)
	$(AR) $(TOBJ_DIR)/$(TOBJ_ARNAME) $^
	
	$(ECHO) $(COLOR_TXT)"\t\t- DYNAMIC\t===>\t"$(COLOR_TAG)"$(TOBJ_NAME)"$(COLOR_NONE)
	$(LN) -o $(TOBJ_DIR)/$(TOBJ_DLNAME) $^ $(BUILD_LIBS)
	
	$(INSTALL_DYLIB_FILE)
	$(ECHO) $(COLOR_TXT)"\t\tBUILD\t >>> "$(COLOR_RED)"$@"$(COLOR_TXT)" <<< COMPLETE"$(COLOR_NONE)


clean :
	@-rm -rf $(SOBJ_DIR) $(TOBJ_DIR)/*
	$(ECHO) $(COLOR_TXT)"\t\tCLEAN\t >>> "$(COLOR_RED)"$@"$(COLOR_TXT)" <<< COMPLETE"$(COLOR_NONE)

distclean : clean
	@-rm -rf $(TOBJ_DIR)
	@-rm -rf $(INSTALL_HEAD_DIR)
	$(ECHO) $(COLOR_TXT)"\t\tCLEAN\t >>> "$(COLOR_RED)"$@"$(COLOR_TXT)" <<< COMPLETE"$(COLOR_NONE)

install : $(TOBJ)
	$(ECHO) $(COLOR_TXT)"\t\t- INSTALL\t===>\t$<"$(COLOR_NONE)
ifeq ($(BUILD_HOST), darwin)
	
else
	
endif
