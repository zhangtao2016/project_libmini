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
COLOR_FILE_OBJ =	$(COLOR_CYAN)
COLOR_FILE_TAG =	$(COLOR_PURPLE)

#######
BUILD_PWD = 		$(shell pwd)
BUILD_HOST =		$(shell uname | tr '[A-Z]' '[a-z]')

#######

BUILD_CFLAGS =		-Wall
BUILD_LIBS =		-lpthread -lm -ldl
BUILD_INC_DIR = 	-I/usr/include -I/usr/local/include
BUILD_LIB_DIR =		-L/usr/local/lib -L/usr/lib -L/usr/libexec

ADD_DYLIBS =
ADD_ARLIBS =		-levcoro -lzmq -lev -lcoro -lparser -lmini -lframe

ADD_INC_DIR = 		-I$(BUILD_PWD)/../../include/libmini \
			-I$(BUILD_PWD)/../../include/libframe \
			-I$(BUILD_PWD)/../../include/libev \
			-I$(BUILD_PWD)/../../include/libcoro \
			-I$(BUILD_PWD)/../../include/libevcoro \
			-I$(BUILD_PWD)/../../include/http-parser \
			-I$(BUILD_PWD)/../../include/redis-parser \
			-I$(BUILD_PWD)/../../include/luajit-2.0			
			
ADD_LIB_DIR =		-L$(BUILD_PWD)/../../lib 

#######

ifeq ($(BUILD_HOST), darwin)
ECHO =			@echo
else
BUILD_LIB_DIR +=	-L/usr/local/lib64 -L/usr/lib64
ECHO =			@echo -e
# BUILD_OPTIMIZE =	-fprofile-arcs
BUILD_INC_DIR +=	-I/usr/local/include/luajit-2.0
ADD_INC_DIR +=		-I$(BUILD_PWD)/../../lib/libkv/release
ADD_LIB_DIR +=		-L$(BUILD_PWD)/../../lib/libkv/release
# ADD_DYLIBS += 	-lgcov -lrt
ADD_ARLIBS += 		
ADD_DYLIBS +=		-lrt
BUILD_CFLAGS +=		-D_GNU_SOURCE -export-dynamic
DYLIB_CFLAGS +=		-Wl,-rpath,$(BUILD_PWD)/../../lib
endif

#######

CC_COMPILER ?=		@gcc
CC =			$(CC_COMPILER) $(BUILD_CFLAGS) $(CFLAGS) $(BUILD_INC_DIR) $(ADD_INC_DIR) $(BUILD_OPTIMIZE)
LN = 			$(CC_COMPILER) $(BUILD_CFLAGS) $(CFLAGS) $(DYLIB_CFLAGS) $(BUILD_LIB_DIR)
AR = 			@ar -rcs

.SUFFIXES:
.SUFFIXES: .o .c

.c.o:
	$(ECHO) $(COLOR_GREEN)"\t\t- COMPILE\t===>\t"$(COLOR_FILE_OBJ)"$(<:.c=.o)"$(COLOR_NONE)
	$(CC) -o $(<:.c=.o) -c $<

#######
SFILE_BASE_DIR = 	src
BIN_DIR = 		../../bin

BIN = $(addprefix $(BIN_DIR)/, \
	cache_test \
	tcp_client_test \
	tcp_server_test \
	tcp_libevserver_test \
	http_client_test \
	http_server_test \
	redis_request_test\
	redis_client_test \
	redis_server_test \
	)

ifeq ($(BUILD_HOST), linux)
BIN += $(addprefix $(BIN_DIR)/, \
	)
endif

define compile_link_obj
	$(ECHO) $(COLOR_GREEN)"\t\t-   BUILD\t===>\t"$(COLOR_FILE_TAG)"$(2)"$(COLOR_NONE)
	$(LN) $(ADD_LIB_DIR) $(1) -o $(2) $(ADD_DYLIBS) $(BUILD_LIBS) $(ADD_ARLIBS)
endef

all : prepare $(BIN)

prepare :
	@-if [ ! -d $(BIN_DIR) ];then mkdir $(BIN_DIR); fi

clean :
	@-find . -name "*.o" -exec rm {} \;
	@-rm -f $(BIN_DIR)/*
	$(ECHO) $(COLOR_GREEN)"\n\tCLEAN\t >>> "$(COLOR_RED)"$@"$(COLOR_GREEN)" <<< COMPLETE"$(COLOR_NONE)

distclean : clean
	@-rm -rf $(BIN_DIR)
	$(ECHO) $(COLOR_GREEN)"\n\tDISTCLEAN\t >>> "$(COLOR_RED)"$@"$(COLOR_GREEN)" <<< COMPLETE"$(COLOR_NONE)

#######以下是你需要修改添加的#######

$(BIN_DIR)/cache_test : $(SFILE_BASE_DIR)/cache_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/tcp_client_test : $(SFILE_BASE_DIR)/tcp_client_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/tcp_server_test : $(SFILE_BASE_DIR)/tcp_server_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/tcp_libevserver_test : $(SFILE_BASE_DIR)/tcp_libevserver_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/http_client_test : $(SFILE_BASE_DIR)/http_client_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/http_server_test : $(SFILE_BASE_DIR)/http_server_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/redis_request_test : $(SFILE_BASE_DIR)/redis_request_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/redis_client_test : $(SFILE_BASE_DIR)/redis_client_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/redis_server_test : $(SFILE_BASE_DIR)/redis_server_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/connection_pool_client_test : $(SFILE_BASE_DIR)/connection_pool_client_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/connection_pool_server_test : $(SFILE_BASE_DIR)/connection_pool_server_test/main.o
	$(call compile_link_obj, $<, $@)

#######	
