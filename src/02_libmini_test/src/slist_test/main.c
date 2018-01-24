#include "libmini.h"

// #define reversestack 1

#define SHM_KEY_ID      (0xf0000002)

#define TOTAL           (20)
#define SIZE            (128)
#define RANGE           (20)

bool travellist(const char *data, int size, void *user);

struct delarg
{
	SListT  list;
	int     value;
};

bool delfindall(const char *data, int size, void *user);

bool delall(const char *data, int size, void *user);

void listtest(SListT list);

void stacktest(SListT list);

void queuetest(SListT list);

void hashtest(SListT list);

void normaltest(SListT list);

int main(int argc, char **argv)
{
	char    *ptr = NULL;
	SListT  list = NULL;
	long    size = 0;
	int     i = 100;

	Rand();

	size = SListCalculateSize(TOTAL, SIZE);

	NewArray0(size, ptr);
	AssertError(ptr, ENOMEM);

	list = SListInit(ptr, size, SIZE, false);
	ASSERTOBJ(list);

	while (i-- > 0) {
		hashtest(list);
		//                        listtest(list);
		//                queuetest(list);

		//                        normaltest(list);
	}

	return EXIT_SUCCESS;
}

void normaltest(SListT list)
{
	int             i = 0;
	struct delarg   arg = { list, 0 };

	//        printf("------------------\n");

	for (i = 0; i < list->totalnodes; i++) {
		//                int     value = RandInt(1, RANGE);
		int value = i + 1;

		if (!SListAddHead(list, (char *)&value, sizeof(value), NULL, NULL)) {
			break;
		}
	}

	printf("------------------\n");

	SListTravel(list, travellist, NULL);

	printf("\n");

	SListTravel(list, delall, &arg);

	printf("\n");

	printf("------------------\n");
}

void listtest(SListT list)
{
	int i = 0;

	printf("------------------\n");

	for (i = 0; i < list->totalnodes; i++) {
		int value = RandInt(1, RANGE);

		if (!SListAddHead(list, (char *)&value, sizeof(value), NULL, NULL)) {
			break;
		}
	}

	printf("sta :");

	SListTravel(list, travellist, NULL);

	printf("\n");

	printf("------------------\n");

	for (i = list->totalnodes; i > 0; i--) {
		int             value = RandInt(1, RANGE);
		int             old = value;
		struct delarg   arg = { list, old };

		printf("del :");

		SListTravel(list, delfindall, &arg);

		printf("\n");

		printf("------------------\n");

		value = RandInt(1, RANGE);

		SListAddTail(list, (char *)&value, sizeof(value), NULL, NULL);

		printf("d : %02d , a : %02d -->>\n", old, value);

		printf("tra :");

		SListTravel(list, travellist, NULL);

		printf("\n");

		printf("------------------\n");
	}

	printf("------------------\n");

	printf("end :");

	SListReverseTravel(list, travellist, NULL);

	printf("\n");
}

void stacktest(SListT list)
{
	int i = 0;

	printf("------------------\n");

	for (i = 0; i < TOTAL; i++) {
		int value = RandInt(1, RANGE);
#ifdef reversestack
		SListAddHead(list, (char *)&value, sizeof(value), NULL, NULL);
#else
		SListAddTail(list, (char *)&value, sizeof(value), NULL, NULL);
#endif
		printf("[%02d]", value);
	}

	printf("\n------------------\n");

	SListReverseTravel(list, travellist, NULL);

	printf("\n");

	for (i = 0; i < TOTAL; i++) {
		int value = -1;
#ifdef reversestack
		SListDelHead(list, (char *)&value, sizeof(value), NULL);
#else
		SListDelTail(list, (char *)&value, sizeof(value), NULL);
#endif
		printf("[%02d]", value);
	}

	printf("\n------------------\n");

#ifdef reversestack
	SListReverseTravel(list, travellist, NULL);
#else
	SListTravel(list, travellist, NULL);
#endif
	printf("\n");
}

void queuetest(SListT list)
{
	int i = 0;

	printf("------------------\n");

	for (i = 0; i < TOTAL; i++) {
		int value = RandInt(1, RANGE);
#ifdef reversestack
		SListAddTail(list, (char *)&value, sizeof(value), NULL, NULL);
#else
		SListAddTail(list, (char *)&value, sizeof(value), NULL, NULL);
#endif
		printf("[%02d]", value);
	}

	printf("\n------------------\n");

	SListReverseTravel(list, travellist, NULL);

	printf("\n");

	for (i = 0; i < TOTAL; i++) {
		int value = -1;
#ifdef reversestack
		SListDelTail(list, (char *)&value, sizeof(value), NULL);
#else
		SListDelHead(list, (char *)&value, sizeof(value), NULL);
#endif
		printf("[%02d]", value);
	}

	printf("\n------------------\n");

#ifdef reversestack
	SListReverseTravel(list, travellist, NULL);
#else
	SListTravel(list, travellist, NULL);
#endif
	printf("\n");
}

struct KY
{
	int     key;
	char    value[5];
};

struct KYW
{
	int     key;
	char    *node;
	int     size;
};

bool findkey(char *data, int size, void *user)
{
	struct KY       *ky = (struct KY *)data;
	struct KYW      *kyw = (struct KYW *)user;

	if (!user) {
		return false;
	}

	if (kyw->key == ky->key) {
		kyw->node = data;
		kyw->size = size;
		return false;
	}

	return true;
}

int lookupkey(const char *data, int size, void *user, int usrsize)
{
	struct KY *ky = (struct KY *)data;

	assert(size >= (int)sizeof(*ky));

	assert(user);
	assert(usrsize == sizeof(int));

	if (ky->key == *(int *)user) {
		return 0;
	}

	return -1;
}

bool travelhash(const char *data, int size, void *user)
{
	struct KY *ky = (struct KY *)data;

	printf("[%02d] = [%.*s]\n", ky->key, size - (int)sizeof(int), ky->value);

	return true;
}

void hashtest(SListT list)
{
	int i = 0;

	printf("------------------\n");

	for (i = 0; i < list->totalnodes; i++) {
		struct KY       ky = {};
		int             j = 0;
		const char      *data = NULL;
		int             size = 0;

		ky.key = RandInt(1, RANGE);

		for (j = 0; j < DIM(ky.value); j++) {
			ky.value[j] = RandInt('a', 'z');
		}

		ky.value[j - 1] = '\0';

		data = SListLookup(list, lookupkey, &ky.key, sizeof(ky.key), &size);

		if (data) {
			struct KY *old = (struct KY *)data;
			printf("update [%02d] = [%.*s] -> [%s]\n",
				old->key,
				size - (int)sizeof(int),
				old->value,
				ky.value);
			memcpy(old->value, ky.value, strlen(ky.value));
		} else {
			printf("insert [%02d] = [%s]\n", ky.key, ky.value);
			SListAddHead(list, (char *)&ky, sizeof(ky), NULL, NULL);
		}
	}

	printf("------------------\n");

	for (i = 0; i < list->totalnodes; i++) {
		struct KY       ky = {};
		const char      *data = NULL;
		int             size = 0;

		ky.key = RandInt(1, RANGE);

		AO_ThreadSpinLock(&list->lock);

		data = SListLookup(list, lookupkey, &ky.key, sizeof(ky.key), &size);

		if (data) {
			struct KY *old = (struct KY *)data;
			printf("find [%02d] = [%.*s]\n",
				old->key,
				size - (int)sizeof(int), old->value);

			SListDelData(list, data);
		} else {
			printf("not find [%02d]\n", ky.key);
		}

		AO_ThreadSpinUnlock(&list->lock);
	}

	printf("------------------\n");

	SListTravel(list, travelhash, NULL);

	printf("\n");
}

bool travellist(const char *data, int size, void *user)
{
	int *value = (int *)data;

	assert(sizeof(int) == size);

	printf("[%02d]", *value);

	return true;
}

bool delfindall(const char *data, int size, void *user)
{
	int             *value = (int *)data;
	struct delarg   *arg = (struct delarg *)user;

	assert(sizeof(int) == size);

	if (arg->value == *value) {
		printf("[%02d]", *value);

		SListDelData(arg->list, data);
	}

	return true;
}

bool delall(const char *data, int size, void *user)
{
	int             *value = (int *)data;
	struct delarg   *arg = (struct delarg *)user;

	assert(sizeof(int) == size);

	printf("[%02d]", *value);

	SListDelData(arg->list, data);

	return true;
}

