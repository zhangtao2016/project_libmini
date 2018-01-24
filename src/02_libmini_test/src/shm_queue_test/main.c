#include "libmini.h"

#define SHM_KEY_ID      (0x00eeddcc)

#define TOTAL           (20)
#define SIZE            (121)

struct KY
{
	int     key;
	char    value[32];
};

bool travellist(const char *data, int size, void *user)
{
	struct KY *ky = (struct KY *)data;

	assert(size = sizeof(struct KY));

	printf("%s : [%d] = [%.*s]\n", (char *)user,
		ky->key, (int)sizeof(ky->value), ky->value);

	return true;
}

void destroynode(const char *data, int size)
{
	struct KY *ky = (struct KY *)data;

	assert(size = sizeof(struct KY));

	printf("destory : [%d] = [%.*s]\n",
		ky->key, (int)sizeof(ky->value), ky->value);
}

int main(int argc, char const *argv[])
{
	ShmQueueT       queue = NULL;
	char            prg[16] = {};
	int             loops = 0;
	int             *counter = NULL;

	GetProgName(prg, sizeof(prg));

	if (unlikely(argc != 4)) {
		x_printf(E, "Usage %s <create/load> <pull/push> <#loops>", prg);
		return EXIT_FAILURE;
	}

	Rand();

	if (strcasecmp(argv[1], "create") == 0) {
		queue = SHM_QueueCreate(SHM_KEY_ID, TOTAL, SIZE);
	} else {
		queue = SHM_QueueCreate(SHM_KEY_ID, 0, 0);
	}

	return_val_if_fail(queue, EXIT_FAILURE);

	counter = (int *)&queue->data->usr[0];
	loops = atoi(argv[2]);

	while (1) {
		char            buff[SIZE] = { 0 };
		struct KY       *ky = (struct KY *)&buff[0];
		int             size = 0;

		if (strcasecmp(argv[2], "pull") == 0) {
			size = 0;

			if (!SHM_QueuePull(queue, buff, sizeof(buff), &size)) {
				/* code */
				futex_wait(&queue->data->nodes, 0, 10);
				continue;
			} else {
				assert(size = sizeof(struct KY));
				fprintf(stderr,
					LOG_W_COLOR
					"-> [%d] = [%.*s]"
					PALETTE_NULL
					"\n",
					ky->key, (int)sizeof(ky->value), ky->value);

				if (unlikely(queue->data->nodes + 6 > queue->data->totalnodes)) {
					futex_wake(&queue->data->nodes, 1);
				}
			}
		} else if (strcasecmp(argv[2], "push") == 0) {
			//                        ky->key = RandInt(1, 100);
			ky->key = ATOMIC_GET(counter);
			snprintf(ky->value, sizeof(ky->value), "-%d", ky->key);

			if (!SHM_QueuePush(queue, buff, sizeof(buff), &size)) {
				/* code */
				futex_wait(&queue->data->nodes, queue->data->capacity, 10);
				continue;
			} else {
				assert(size = sizeof(struct KY));
				fprintf(stderr,
					LOG_E_COLOR
					"[%d] = [%.*s] ->"
					PALETTE_NULL
					"\n",
					ky->key, (int)sizeof(ky->value), ky->value);

				if (unlikely(queue->data->nodes < 6)) {
					futex_wake(&queue->data->nodes, 1);
				}

				ATOMIC_INC(counter);
			}
		} else {
			x_printf(E, "Usage %s <create/load> <pull/push> <#loops>", prg);
			break;
		}

		//                usleep(100000);

		if (--loops == 0) {
			break;
		}
	}

	SHM_QueueDestroy(&queue, true, destroynode);

	return 0;
}

