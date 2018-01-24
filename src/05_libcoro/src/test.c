#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include "coro.h"
#define RandInt(low, high) \
        ((long)((((double)random()) / ((double)RAND_MAX + 1)) * (high - low + 1)) + low)

/*
 * 随机获取小数 x ： low <= x < high
 */
#define RandReal(low, high) \
        ((((double)random()) / ((double)RAND_MAX + 1)) * (high - low) + low)

/*
 * 测试一次概率为 p(分数) 的事件
 */
#define RandChance(p) (RandReal(0, 1) < (double)(p) ? true : false)

#if 0
coro_context ctx1, ctx2, mainctx;
struct coro_stack stack1, stack2;

void coro_body1(void *arg)
{
        printf("1 OK\n");
        coro_transfer(&ctx1, &ctx2);
        printf("1 Back in coro\n");
        coro_transfer(&ctx1, &ctx2);
        printf("?????\n");
}

void coro_body2(void *arg)
{
        printf("2 OK\n");
        coro_transfer(&ctx2, &mainctx);
        printf("2 Back in coro\n");
        coro_transfer(&ctx2, &mainctx);
        printf("?????\n");
}

int main(int argc, char **argv)
{
        coro_create(&mainctx, NULL, NULL, NULL, 0);

        coro_stack_alloc(&stack1, 0);
        coro_create(&ctx1, coro_body1, NULL, stack1.sptr, stack1.ssze);

        coro_stack_alloc(&stack2, 0);
        coro_create(&ctx2, coro_body2, NULL, stack2.sptr, stack2.ssze);

        printf("Created a coro\n");
        coro_transfer(&mainctx, &ctx1);
        printf("Back in main\n");
        coro_transfer(&mainctx, &ctx1);
        printf("Back in main again\n");
        return 0;
}

#else

  #include <unistd.h>
  #include <stdlib.h>
  #include <stdio.h>
  #include <string.h>
  #include "coro_cluster.h"

/* ---------------              */

static int task_id = 0;

struct taskargs
{
        int     id;
        int     cycle;
        int     rc;
        bool    m;
};

void taskfree(void *usr)
{
        struct taskargs *task = usr;

        if (task && task->m) {
                printf("\033[1;31m" "Free task [%p] when destroy a cluster." "\033[m" "\n", task);
                free(task);
        }
}

void testcleanup(void *usr)
{
        if (usr) {
                printf("\033[1;31m" "free [%p] when a task exit." "\033[m" "\n", usr);
                free(usr);
        }
}

int testtask(struct coro_cluster *coroptr, void *usr)
{
        struct taskargs *arg = usr;
        int             rc = 0;
        int             i = 0;
        char            *unused = NULL;
        unused = calloc(10, sizeof(*unused));
        /*测试清理栈*/
        coro_cluster_cleanpush(coroptr, testcleanup, unused);

        printf("\033[1;33m" "ID %d start ..." "\033[m" "\n", arg->id);

        if (arg->cycle < 1) {
                printf("\033[1;31m" "ID %d exit ..." "\033[m" "\n", arg->id);
                return -1;
        }

  #if 1
        for (i = 0; i < arg->cycle; i++)
  #else
        /*测试超时，随机运行一段时间*/
        coro_cluster_timeout(coroptr, (int)RandInt(500, 1000), false);
        for (i = 0; ; i++)
  #endif
        {
  #if 0
                coro_cluster_switch(coroptr);
                //usleep(50000);
  #else
                /**/
                bool idle = RandChance(6.0 / 7.0);

                //printf("~~~ IDLE %s ~~~\n", idle ? "true" : "fasle");
                if (idle) {
                        coro_cluster_idleswitch(coroptr);
                } else {
                        coro_cluster_switch(coroptr);
                }
  #endif
                printf("\033[1;32m" "ID %d : %d" "\033[m" "\n", arg->id, i + 1);
        }

        printf("\033[1;33m" "ID %d end ..." "\033[m" "\n", arg->id);

        rc = arg->rc;
        coro_cluster_cleanpop(coroptr, true);
        taskfree(arg);
        return rc;
}

void idle(struct coro_cluster *cluster, void *usr)
{
        struct timeval  tv = { };
        gettimeofday(&tv, NULL);
        
        printf("\033[1;31m" "<<< %ld.%06d : IDLE, tasks [%ld] >>>" "\033[m" "\n",
               tv.tv_sec, tv.tv_usec, cluster->tasks);
  #if 0
        /*可以在idle中添加新的任务*/
        static int      i = 0;

        if ((i++ % 2) == 0) {
                struct taskargs *args = NULL;

                args = calloc(1, sizeof(*args));
                args->m = true;
                args->id = task_id++;
                args->cycle = (int)RandInt(150, 200);
                args->rc = 0;

                coro_cluster_add(cluster, testtask, args, 0);
                coro_cluster_switch(cluster);
        }

    #if 0
        if (i >= 300) {
                /*可以停止整个协程集群*/
                coro_cluster_stop(cluster);
        }
    #endif
  #endif /* if 1 */
}

int main()
{
        struct coro_cluster *cluster = NULL;

  #if 1
        cluster = coro_cluster_create(-1, false);
        struct taskargs task[] = {
                { .cycle = 2,  .m = false },
                { .cycle = 4,  .m = false },
                { .cycle = 6,  .m = false },
                { .cycle = 3,  .m = false },
                { .cycle = 10, .rc = -1, .m = false },
                { .cycle = 12, .m = false },
                { .cycle = 14, .m = false },
        };

        int     i = 0;
        int     tasks = (int)(sizeof(task) / sizeof(task[0]));

        for (i = 0; i < tasks; i++) {
                task[i].id = task_id++;
                coro_cluster_add(cluster, testtask, &task[i], 0);
        }

  #else
        /*可以不添加任何任务，然后空转循环启动，在idle中添加任务*/
        cluster = coro_cluster_create(-1, true);
  #endif /* if 1 */
        bool rc = false;
  #if 1
        rc = coro_cluster_startup(cluster, idle, NULL);
  #else
        /*如果空转启动，但不给出idle回调，循环将立马退出*/
        rc = coro_cluster_startup(cluster, NULL, NULL);
  #endif

        if (!rc) {
                printf("\033[1;31m" "task cluster execute failure" "\033[m" "\n");
        } else {
                printf("\033[1;32m" "task cluster execute success" "\033[m" "\n");
        }

  #if 1
        /*协程集群可再次被启动，［一般先判断是否时调用coro_cluster_stop()而停止]*/
        if (coro_cluster_isactive(cluster)) {
                printf("\033[1;32m" "task cluster run again" "\033[m" "\n");
                coro_cluster_startup(cluster, idle, NULL);
        }
  #endif
        printf("\033[1;33m" "task cluster destroy" "\033[m" "\n");
        /*销毁集群*/
        coro_cluster_destroy(cluster, taskfree);

        return EXIT_SUCCESS;
}
#endif /* if 0 */