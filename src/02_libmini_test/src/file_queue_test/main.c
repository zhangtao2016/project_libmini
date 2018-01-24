#include "libmini.h"

#define FILE_QUEUE_NAME ("file_queue.data")

#define TOTAL           (200)
#define SIZE            (36)

struct KY
{
	int     key;
	char    value[32];
};

bool travellist(char *data, int size, void *user)
{
	struct KY *ky = (struct KY *)data;

	assert(size = sizeof(struct KY));

	x_printf(D, "%s : [%d] = [%.*s]", (char *)user,
		ky->key, (int)sizeof(ky->value), ky->value);

	return true;
}

void destroynode(const char *data, int size)
{
	struct KY *ky = (struct KY *)data;

	assert(size = sizeof(struct KY));

	x_printf(D, "destory : [%d] = [%.*s]",
		ky->key, (int)sizeof(ky->value), ky->value);
}

int main(int argc, char const *argv[])
{
	FileQueueT      queue = NULL;
	char            prg[16] = {};
	char            log[32] = {};
	int             loops = 0;
	int             *counter = NULL;

	GetProgName(prg, sizeof(prg));

	if (unlikely(argc != 4)) {
		x_printf(E, "Usage %s <create/load> <pull/push> <#loops>", prg);
		return EXIT_FAILURE;
	}

	snprintf(log, sizeof(log), "%s.%d", prg, getpid());

	SLogOpen(log, SLOG_D_LEVEL);

	Rand();

	if (strcasecmp(argv[1], "create") == 0) {
		queue = File_QueueCreate(FILE_QUEUE_NAME, TOTAL, SIZE);
	} else {
		queue = File_QueueCreate(FILE_QUEUE_NAME, 1, 0);
	}

	return_val_if_fail(queue, EXIT_FAILURE);

	counter = (int *)&queue->data->usr[0];

	loops = atoi(argv[3]);

	while (1) {
		char            buff[SIZE] = { 0 };
		struct KY       *ky = (struct KY *)&buff[0];
		int             size = 0;

		if (strcasecmp(argv[2], "pull") == 0) {
			if (!File_QueuePull(queue, buff, sizeof(buff), &size)) {
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
			struct KY *ky = (struct KY *)&buff[0];

			//                        ky->key = RandInt(1, 100);
			ky->key = ATOMIC_GET(counter);

			snprintf(ky->value, sizeof(ky->value), "-%d", ky->key);

			if (!File_QueuePush(queue, buff, sizeof(buff), &size)) {
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
			x_printf(E, "usage %s <create/load> <pull/push> <#loops>.", prg);
			break;
		}

		//                usleep(100000);

		if (--loops == 0) {
			break;
		}
	}

	File_QueueDestroy(&queue, false, destroynode);

	return 0;
}

