#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <libkv.h>

#include "libmini.h"

/*
 * libkv句柄池
 */
struct kvpool_handle
{
	char    name[64];
	void    *hdl;
};

extern SListT g_kvhandle_pool;

/**
 * 初始化
 */
static bool kvpool_init();

/**
 * 在lua层以句柄名称调用，查找或创建对应的句柄，并返回该句柄的libkv句柄的C指针
 */
static int search_kvhandle(lua_State *L);

/**
 * 在C层以句柄名称调用，查找或创建对应的句柄，并返回该句柄
 */
static struct kvpool_handle     *kvpool_search(SListT list, const char *name);

static int _getProgName(lua_State *L);

static void *work(void *arg);

SListT          g_kvhandle_pool = NULL;
volatile int    g_startflag = 0;
volatile int    g_workers = 0;

int main(int argc, char **argv)
{
	char            prg[32] = {};
	pthread_t       *tid = NULL;
	int             tids = 0;
	int             i = 0;

	if (unlikely(argc != 3)) {
		x_printf(E, "Usage %s <#threads> <lua-script>.", GetProgName(prg, sizeof(prg)));
		return EXIT_FAILURE;
	}

	tids = atoi(argv[1]);
	tids = tids < 1 ? 1 : tids;

	if (unlikely(!kvpool_init())) {
		x_printf(E, "Can't initialize libkv-pool.");
		return EXIT_FAILURE;
	}

	NewArray0(tids, tid);
	AssertError(tid, ENOMEM);

	for (i = 0; i < tids; i++) {
		pthread_create(&tid[i], NULL, work, argv[2]);
	}

	futex_cond_wait((int *)&g_workers, tids, -1);
	futex_set_signal((int *)&g_startflag, 1, tids);

	for (i = 0; i < tids; i++) {
		pthread_join(tid[i], NULL);
	}

	return EXIT_SUCCESS;
}

static void *work(void *arg)
{
	const char      *file = (const char *)arg;
	lua_State       *L = NULL;
	int             flag = 0;
	char            tmstr[32] = {};
	char            *tmptr = NULL;
	struct timeval  tv1 = {};
	struct timeval  tv2 = {};
	struct timeval  tv = {};

	assert(file);

	BindCPUCore(-1);

	L = luaL_newstate();

	assert(L);
	luaopen_base(L);
	luaL_openlibs(L);

	lua_register(L, "getprogname", _getProgName);
	lua_register(L, "search_kvhandle", search_kvhandle);

	futex_add_signal((int *)&g_workers, 1, 1);
	futex_cond_wait((int *)&g_startflag, 1, -1);

	gettimeofday(&tv1, NULL);

	x_printf(D, ">>>>> START <<<<<");

	flag = luaL_dofile(L, file);

	if (unlikely(flag != 0)) {
		x_printf(E, "Can't load lua-script file %s : %s", file, luaL_checkstring(L, -1));
	}

	gettimeofday(&tv2, NULL);

	x_printf(D, ">>>>> END   <<<<<");

	tv.tv_sec = tv2.tv_sec - tv1.tv_sec;
	tv.tv_usec = tv2.tv_usec - tv1.tv_usec;

	if (tv.tv_usec < 0) {
		tv.tv_sec -= 1;
		tv.tv_usec += 1000000;
	}

	tmptr = TM_FormatTime(tmstr, sizeof(tmstr), tv.tv_sec, "%M:%S");

	x_printf(W, "Escape time : %s.%06d", tmptr ? tmptr : 0, (int)tv.tv_usec);

	return NULL;
}

static int _kvpool_find4name(const char *buff, int size, void *user, int usrsize)
{
	struct kvpool_handle    *node = NULL;
	const char              *name = NULL;

	assert(user);
	name = (const char *)user;

	if (usrsize == 0) {
		return -1;
	}

	assert(size == sizeof(*node));
	node = (struct kvpool_handle *)buff;

	if (strcmp(node->name, name) == 0) {
		return 0;
	}

	return -1;
}

bool kvpool_init()
{
	char    *buff = NULL;
	long    size = 0;

	size = SListCalculateSize(128, (int)sizeof(struct kvpool_handle));
	NewArray0(size, buff);
	assert(buff);

	g_kvhandle_pool = SListInit(buff, size, (int)sizeof(struct kvpool_handle), false);

	if (g_kvhandle_pool == NULL) {
		x_printf(E, "Can't initalize the pool of libkv handler.");
		Free(buff);

		return false;
	}

	return true;
}

struct kvpool_handle *kvpool_search(SListT list, const char *name)
{
	struct kvpool_handle    hdl = {};
	struct kvpool_handle    *ptr = NULL;
	int                     ptrsize = 0;

	SListLock(list);

	ptr = (struct kvpool_handle *)SListLookup(list, _kvpool_find4name, (void *)name, (int)strlen(name), &ptrsize);

	if (ptr) {
		assert(ptrsize == sizeof(*ptr));
		SListUnlock(list);
		return ptr;
	}

	snprintf(hdl.name, sizeof(hdl.name), "%s", name);
	hdl.hdl = (void *)kv_create(NULL);

	if (SListAddHead(list, (char *)&hdl, sizeof(hdl), &ptrsize, (void **)&ptr)) {
		assert(ptrsize == sizeof(*ptr));
	} else {
		x_printf(E, "can't add a handle of libkv by %s.", name);
		ptr = NULL;
	}

	SListUnlock(list);

	return ptr;
}

int search_kvhandle(lua_State *L)
{
	int                     argc = lua_gettop(L);
	const char              *name = NULL;
	struct kvpool_handle    *hdl = NULL;

	if (argc != 1) {
		luaL_error(L, "Bad arguments, `name` expected.");
	}

	name = luaL_checkstring(L, 1);

	if (name[0] == '\0') {
		luaL_error(L, "Bad arguments, `name` is null.");
	}

	hdl = kvpool_search(g_kvhandle_pool, name);

	if (!hdl) {
		lua_pushnil(L);
	} else {
		lua_pushlightuserdata(L, hdl->hdl);
	}

	return 1;
}

static int _getProgName(lua_State *L)
{
	char            buff[32] = {};
	const char      *prog = NULL;

	prog = GetProgName(buff, sizeof(buff));

	lua_pushstring(L, prog);

	return 1;
}

