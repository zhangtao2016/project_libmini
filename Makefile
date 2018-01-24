COLOR_NONE      = "\033[m"
COLOR_GRAY      = "\033[1;30m"
COLOR_RED       = "\033[1;31m"
COLOR_GREEN     = "\033[1;32m"
COLOR_YELLOW    = "\033[1;33m"
COLOR_BLUE      = "\033[1;34m"
COLOR_PURPLE    = "\033[1;35m"
COLOR_CYAN      = "\033[1;36m"
COLOR_WHITE     = "\033[1;37m"

#######
BIN_DIR 	= bin
LIB_DIR 	= lib
OBJ_DIR 	= .objs
DOC_DIR 	= doc
INC_DIR 	= include
SRC_DIR		= src
INSTALL		= install

#######
SFILE_MAKEFILE	= $(shell find ./$(SRC_DIR) -type f -name "[Mm]akefile" | sort)
SUBDIRS		= $(dir $(SFILE_MAKEFILE))

SFILE_SHELL	= $(shell find ./$(INSTALL) -type f -name "makefile.sh" | sort)
###################
HEAD_MODE_0=
HEAD_MODE_1="================================"

define print_head_base
	@echo $(1)$(2)$(3)$(2)$(COLOR_NONE)
endef

define view_the_current_system_information
	$(call print_head_base, $(COLOR_YELLOW),$(HEAD_MODE_1),"获取信息")
	$(call print_head_base, $(COLOR_RED),$(HEAD_MODE_0),"查看linux发行版本")
	@echo $(shell lsb_release -d)
	$(call print_head_base, $(COLOR_RED),$(HEAD_MODE_0),"查看操作系统的位数")		
	@echo $(shell getconf LONG_BIT)
	$(call print_head_base, $(COLOR_RED),$(HEAD_MODE_0),"查看linux内核版本号")	
	@echo $(shell uname -sr)
	$(call print_head_base, $(COLOR_RED),$(HEAD_MODE_0),"查看gcc版本")	
	@echo `gcc  --version`
	$(call print_head_base, $(COLOR_RED),$(HEAD_MODE_0),"查看make版本")	
	@echo `make --version`
	$(call print_head_base, $(COLOR_YELLOW),$(HEAD_MODE_1),"开始编译")
endef
###################
define create_folder_base
	@-if [ ! -d $(1) ];then mkdir $(1); fi
endef

define create_folder_pack
	$(call create_folder_base, $(BIN_DIR))
	$(call create_folder_base, $(LIB_DIR))
	$(call create_folder_base, $(OBJ_DIR))
	$(call create_folder_base, $(RES_DIR))
	$(call create_folder_base, $(DOC_DIR))
	$(call create_folder_base, $(DAT_DIR))
	$(call create_folder_base, $(LOG_DIR))
	$(call create_folder_base, $(INC_DIR))
	$(call create_folder_base, $(SRC_DIR))
endef

###################
define batch_execute_makefile
	@list='$(SUBDIRS)'; \
	for subdir in $$list; \
	do \
		echo $(COLOR_RED)"making $@ in $$subdir"$(COLOR_NONE); \
		( cd $$subdir && $(MAKE) $@ ) || exit 1; \
	done;
endef

define batch_execute_shell
	@list='$(SFILE_SHELL)'; \
	for file in $$list; \
	do \
		echo $(COLOR_RED)"making $@ in $$file"$(COLOR_NONE); \
		( sh $$file ) || exit 1; \
	done;
endef

#######
.PHONY: all clean

all:
	$(view_the_current_system_information)
	$(create_folder_pack)	
	$(batch_execute_makefile)

clean:
	$(batch_execute_makefile)
	
inatall:
	$(batch_execute_shell)	
	
#######
