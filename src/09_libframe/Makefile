INC_DIR=	-I../../include/libframe        \
		-I../../include/libev 		\
		-I../../include/http-parser 	\
		-I../../include/redis-parser 	\
		-I../../include/libevcoro 	\
		-I../../include/libmini 	\
		-I../../include/luajit-2.0	\
		-I../../include/libcoro
	
LIB_DIR=	-L/usr/local/lib                \
		-L../../lib

LIBS=		-lluajit-5.1 -lpthread -lrt -lm

BASE_DIR =	./src
SOBJ_DIR = 	../../.objs
TOBJ_DIR =	../../lib
#---------------------------------------
ECHO = 		@echo -e
COMPILER = 	gcc
AC = 		@ar -rcs

COLOR_OBJ =	"\033[0;36m"
COLOR_TAG =	"\033[1;33m"
COLOR_TXT = 	"\033[1;32m"
COLOR_RED = 	"\033[1;31m"
COLOR_NONE = 	"\033[m"


define compile_obj
	$(ECHO) $(COLOR_TXT)"\t\t- COMPILE\t===>\t"$(COLOR_OBJ)"$(1:.c=.o)"$(COLOR_NONE)
	$(COMPILER) $(CFLAGS) $(INC_DIR) -c $(1) -o $(2)
endef

#---------------------------------------
#外部文件
ADD_OBJ += http_parser.o 
ADD_OBJ += redis_parser.o
#---------------------------------------

##基础库需要的文件

BASE_OBJ = $(addprefix $(SOBJ_DIR)/, \
	$(ADD_OBJ) \
	utils.o \
	cache.o \
	tcp_io.o \
	tcp_base.o \
	http_parse.o \
	http_reqresp.o \
	http_string.o \
	redis_parse.o \
	redis_reqresp.o \
	redis_string.o \
	lj_http.o \
	netpkg_statistic.o \
	pool.o \
	connection_pool.o \
	)

##框架库需要的文件

#---------------------------------------
#需要编译的根目录


#---- 一级目录
CACHE_DIR=$(BASE_DIR)/cache
TCP_API_DIR=$(BASE_DIR)/tcp_api
LUA_EXPAND_DIR=$(BASE_DIR)/lua_expand
POOL_API_DIR=$(BASE_DIR)/pool_api
NETPKG_STAT_DIR=$(BASE_DIR)/netpkg_statistic

#---- 二级目录
CONN_POOL_DIR=$(TCP_API_DIR)/connection_pool
HTTP_API_DIR=$(TCP_API_DIR)/http_api
REDIS_API_DIR=$(TCP_API_DIR)/redis_api
ASYNC_GRPREQ_DIR=$(TCP_API_DIR)/async_grpreq
#---------------------------------------
ifeq ($(LIBNAME),)
LIBNAME = frame
endif

PJT := lib$(LIBNAME).a

#---------------------------------------
#规则标签
all: prepare $(PJT)

prepare:
	@-if [ ! -d $(SOBJ_DIR) ];then mkdir $(SOBJ_DIR); fi
	@-if [ ! -d $(TOBJ_DIR) ];then mkdir $(TOBJ_DIR); fi
	
$(PJT): $(BASE_OBJ)
	$(ECHO) $(COLOR_TXT)"\t\t- ARCHIVE\t===>\t"$(COLOR_TAG)"$(PJT)"$(COLOR_NONE)
	$(AC) $(TOBJ_DIR)/$@ $^
	$(ECHO) $(COLOR_TXT)"\n\t- BUILD\t >>> "$(COLOR_RED)"$@"$(COLOR_TXT)" <<< COMPLETE"$(COLOR_NONE)

#---------------------------------------
#编译框架中相关文件夹下的c文件
#一级目录编译
$(SOBJ_DIR)/%.o : $(BASE_DIR)/%.c
	$(call compile_obj, $^, $@)

$(SOBJ_DIR)/%.o : $(TCP_API_DIR)/%.c
	$(call compile_obj, $^, $@)

$(SOBJ_DIR)/%.o : $(CACHE_DIR)/%.c
	$(call compile_obj, $^, $@)

$(SOBJ_DIR)/%.o : $(LUA_EXPAND_DIR)/%.c
	$(call compile_obj, $^, $@)

$(SOBJ_DIR)/%.o : $(POOL_API_DIR)/%.c
	$(call compile_obj, $^, $@)

$(SOBJ_DIR)/%.o : $(NETPKG_STAT_DIR)/%.c
	$(call compile_obj, $^, $@)

#---------------------------------------
#二级目录编译
$(SOBJ_DIR)/%.o : $(CONN_POOL_DIR)/%.c
	$(call compile_obj, $^, $@)
$(SOBJ_DIR)/%.o : $(HTTP_API_DIR)/%.c
	$(call compile_obj, $^, $@)
$(SOBJ_DIR)/%.o : $(REDIS_API_DIR)/%.c
	$(call compile_obj, $^, $@)

#--------------------------------------
#外部c文件
$(SOBJ_DIR)/redis_parser.o : ../04_redis-parser/redis_parser.c
	$(call compile_obj, $^, $@)

$(SOBJ_DIR)/http_parser.o : ../03_http-parser/http_parser.c
	$(call compile_obj, $^, $@)

#---------------------------------------
#工具程序

#---------------------------------------
clean:
	rm -rf $(SOBJ_DIR)

distclean:
	make clean
	rm -rf ./lib*.a
