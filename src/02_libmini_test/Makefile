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
ADD_ARLIBS =		-lmini
ADD_INC_DIR = 		-I$(BUILD_PWD)/../../include/libmini
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

define compile_link_obj
	$(ECHO) $(COLOR_GREEN)"\t\t-   BUILD\t===>\t"$(COLOR_FILE_TAG)"$(2)"$(COLOR_NONE)
	$(LN) $(ADD_LIB_DIR) $(1) -o $(2) $(ADD_ARLIBS) $(ADD_DYLIBS) $(BUILD_LIBS)
endef

#######
SFILE_BASE_DIR = 	src
BIN_DIR = 		../../bin

BIN = $(addprefix $(BIN_DIR)/, \
	utils_test \
	time_test \
	squeue_test \
	slist_test \
	shm_queue_test \
	file_queue_test \
	log_test \
	expand_slog \
	mmap_test \
	process_lock_test \
	thread_lock_test \
	futex_test \
	bit_test \
	socket_test \
	atomic_list \
	tcp_client_test \
	tcp_server_test \
	dstack_test \
	dqueue_test \
	dhash_test \
	except_test \
	signal_test \
	)

ifeq ($(BUILD_HOST), linux)
BIN += $(addprefix $(BIN_DIR)/, \
	ipc_test \
	coroutine_test \
	coroutine_simple \
	)
endif

#############
all : prepare $(BIN)

prepare :
	@-if [ ! -d $(BIN_DIR) ];then mkdir $(BIN_DIR); fi

clean :
	@-find . -name "*.o" -exec rm {} \;
	@-rm -f $(BIN_DIR)/*
	$(ECHO) $(COLOR_GREEN)"\t\tCLEAN\t >>> "$(COLOR_RED)"$@"$(COLOR_GREEN)" <<< COMPLETE"$(COLOR_NONE)

distclean : clean
	@-rm -rf $(BIN_DIR)
	$(ECHO) $(COLOR_GREEN)"\t\tDISTCLEAN\t >>> "$(COLOR_RED)"$@"$(COLOR_GREEN)" <<< COMPLETE"$(COLOR_NONE)

#######以下是你需要修改添加的#######

$(BIN_DIR)/utils_test : $(SFILE_BASE_DIR)/utils_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/time_test : $(SFILE_BASE_DIR)/time_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/squeue_test : $(SFILE_BASE_DIR)/squeue_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/slist_test : $(SFILE_BASE_DIR)/slist_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/shm_queue_test : $(SFILE_BASE_DIR)/shm_queue_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/file_queue_test : $(SFILE_BASE_DIR)/file_queue_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/log_test : $(SFILE_BASE_DIR)/log_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/expand_slog : $(SFILE_BASE_DIR)/expand_slog/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/ipc_test : $(SFILE_BASE_DIR)/ipc_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/mmap_test : $(SFILE_BASE_DIR)/mmap_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/process_lock_test : $(SFILE_BASE_DIR)/process_lock_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/coroutine_test : $(SFILE_BASE_DIR)/coroutine_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/coroutine_simple : $(SFILE_BASE_DIR)/coroutine_simple/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/thread_lock_test : $(SFILE_BASE_DIR)/thread_lock_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/futex_test : $(SFILE_BASE_DIR)/futex_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/bit_test : $(SFILE_BASE_DIR)/bit_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/socket_test : $(SFILE_BASE_DIR)/socket_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/lua_multi_vm : $(SFILE_BASE_DIR)/lua_multi_vm/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/atomic_list : $(SFILE_BASE_DIR)/atomic_list/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/tcp_server_test : $(SFILE_BASE_DIR)/tcp_server_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/tcp_client_test : $(SFILE_BASE_DIR)/tcp_client_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/dstack_test : $(SFILE_BASE_DIR)/dstack_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/dqueue_test : $(SFILE_BASE_DIR)/dqueue_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/dhash_test : $(SFILE_BASE_DIR)/dhash_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/except_test : $(SFILE_BASE_DIR)/except_test/main.o
	$(call compile_link_obj, $<, $@)

$(BIN_DIR)/signal_test : $(SFILE_BASE_DIR)/signal_test/main.o
	$(call compile_link_obj, $<, $@)

#######
