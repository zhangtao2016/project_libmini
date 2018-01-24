version=0.09

name=lua-redis-parser
dist=$(name)-$(version)

LUA_VERSION =   5.1

# See http://lua-users.org/wiki/BuildingModules for platform specific
# details.

## Linux/BSD
#PREFIX ?=          /usr/local
PREFIX ?=          ../..
LDFLAGS +=         -shared

## OSX (Macports)
#PREFIX ?=          /opt/local
#LDFLAGS +=         -bundle -undefined dynamic_lookup

LUA_INCLUDE_DIR ?= $(PREFIX)/include/luajit-2.0
LUA_LIB_DIR ?=     $(PREFIX)/lib/lua/$(LUA_VERSION)

#CFLAGS ?=          -g -Wall -pedantic -fno-inline
CFLAGS ?=          -g -O -Wall
override CFLAGS += -fpic -I$(LUA_INCLUDE_DIR)

INSTALL ?= install

SOBJ_DIR =		../../.objs
TOBJ_DIR =		../../lib
TOBJ_NAME = 		libparser.so


.PHONY: all clean dist test t

#CC = gcc
RM = rm -f

all: $(TOBJ_NAME)

redis_parser.o: ddebug.h

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(BUILD_CFLAGS) -o $@ $<

$(TOBJ_NAME): redis_parser.o
	$(CC) $(LDFLAGS) -o $(TOBJ_DIR)/$@ $^

install:
	$(INSTALL) -d $(DESTDIR)$(LUA_LIB_DIR)/redis
	$(INSTALL) $(TOBJ_NAME) $(DESTDIR)$(LUA_LIB_DIR)/redis

clean:
	$(RM) *.so *.o redis/*.so

test: all
	$(INSTALL) -d redis
	$(INSTALL) $(TOBJ_NAME) redis/
	prove -r t

valtest: $(TOBJ_NAME)
	$(INSTALL) -d redis
	$(INSTALL) $(TOBJ_NAME) redis/
	TEST_LUA_USE_VALGRIND=1 prove -r t

t: $(TOBJ_NAME)
	$(INSTALL) -d redis
	$(INSTALL) $(TOBJ_NAME) redis/
	TEST_LUA_USE_VALGRIND=1 prove t/sanity.t

dist:
	git archive --prefix="$(dist)/" master | \
		gzip -9 > "$(dist).tar.gz"
	git archive --prefix="$(dist)/" \
		-o "$(dist).zip" master

