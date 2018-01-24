//
//  dhash.c
//  supex
//
//  Created by 周凯 on 15/12/22.
//  Copyright © 2015年 zhoukai. All rights reserved.
//
#include <pthread.h>
#include <math.h>
#include "adt/dynamic/dhash.h"
#include "except/exception.h"
#include "slog/slog.h"
#include "atomic/atomic.h"
struct DHashNode
{
	struct DHashNode        *next;
	uint32_t                size;		/**<总长度*/
	uint32_t                hashv;		/**<hash值*/
	uint32_t                klen;		/**<key长度*/
	uint32_t                vlen;		/**<value长度*/
	char                    data[1];	/**<key/value数据起始位置*/
};

struct DHashBucket
{
	int                     magic;
	int                     size;		/**<节点数组大小*/
	int                     *nodesptr;	/**<节点计数器数组*/
	struct DHashNode        **nodeptr;	/**<节点指针数组*/
	pthread_rwlock_t        *lockerptr;	/**<节点同步锁数组*/
};

#define DHASH_SEARCH_BEGIN(flag, hash, key, klen, startptr, hashvalue, counter)	\
	do {									\
		(klen) = (klen) < 0 ? (int)strlen((key)) : (klen);		\
		int                     _pos = 0;				\
		pthread_rwlock_t        *_lock = NULL;				\
		struct DHashBucket      *_bucket = hash->bucket;		\
		ASSERTOBJ(_bucket);						\
		(hashvalue) = (hash)->hashk((key), (klen));			\
		_pos = (hashvalue) % _bucket->size;				\
		_lock = &_bucket->lockerptr[_pos];				\
		(startptr) = &_bucket->nodeptr[_pos];				\
		(counter) = &_bucket->nodesptr[_pos];				\
		pthread_rwlock_wrlock(_lock);					\
		(flag) = _DHashSearch(&(startptr), (key), (klen), (hashvalue), (hash)->comparek);

#define DHASH_SEARCH_END()	      \
	pthread_rwlock_unlock(_lock); \
	}			      \
	while (0)

static bool _DHashSearch(struct DHashNode ***prevnodeptr,
	const char *key, int klen,
	uint32_t hashvalue, NodeCompareCB compare);

static inline struct DHashNode  *_DHashNodeAllocate(struct DHashNode *old,
	uint32_t ksize, uint32_t vsize);

DHashT DHashCreate(int size, HashCB hash, NodeCompareCB compare)
{
	DHashT volatile                 dhash = NULL;
	struct DHashBucket volatile     *bucket = NULL;

	size = CalculateTableSize(size);

	TRY
	{
		New(bucket);
		AssertError(bucket, ENOMEM);

		bucket->size = size;
		NewArray0(size, bucket->nodesptr);
		NewArray0(size, bucket->nodeptr);
		NewArray0(size, bucket->lockerptr);

		AssertError(bucket->nodeptr &&
			bucket->nodesptr &&
			bucket->lockerptr, ENOMEM);

		int i = 0;

		for (i = 0; i < size; i++) {
			pthread_rwlock_init(&bucket->lockerptr[i], NULL);
		}

		REFOBJ(bucket);
		New(dhash);
		AssertError(dhash, ENOMEM);

		dhash->bucket = (void *)bucket;
		dhash->comparek = compare ? compare : HashCompareDefaultCB;
		dhash->hashk = hash ? hash : HashDefaultCB;
		dhash->nodes = 0;
		REFOBJ(dhash);
	}
	CATCH
	{
		UNREFOBJ(bucket);
		UNREFOBJ(dhash);

		if (bucket) {
			Free(bucket->nodeptr);
			Free(bucket->nodesptr);

			if (bucket->lockerptr) {
				int i = 0;

				for (i = 0; i < size; i++) {
					pthread_rwlock_destroy(&bucket->lockerptr[i]);
				}

				Free(bucket->lockerptr);
			}

			Free(bucket);
		}

		Free(dhash);
	}
	END;

	return dhash;
}

void DHashDestroy(DHashT *hashptr)
{
	assert(hashptr);
	DHashT hash = (*hashptr);
	return_if_fail(UNREFOBJ(hash));

	struct DHashBucket *bucket = hash->bucket;
	ASSERTOBJ(bucket);
	UNREFOBJ(bucket);

	int pos = 0;

	for (pos = 0; pos < bucket->size; pos++) {
		pthread_rwlock_wrlock(&bucket->lockerptr[pos]);
#ifdef DEBUG
		x_printf(D, "this bucket have got %d nodes", bucket->nodesptr[pos]);
#endif
		struct DHashNode        *next = bucket->nodeptr[pos];
		struct DHashNode        *tmp = NULL;

		while (next) {
			/*call destroy back by key and value*/
			if (hash->destroyk) {
				TRY { hash->destroyk(next->data, next->klen); } CATCH {} END;
			}

			if (hash->destroyv) {
				TRY { hash->destroyv(next->data + next->klen, next->vlen); } CATCH {} END;
			}

			/*free memory of node*/
			tmp = next->next;
			free(next);
			next = tmp;
		}

		bucket->nodeptr[pos] = NULL;
		pthread_rwlock_unlock(&bucket->lockerptr[pos]);
		pthread_rwlock_destroy(&bucket->lockerptr[pos]);
	}

	Free(bucket->nodesptr);
	Free(bucket->nodeptr);
	Free(bucket->lockerptr);
	Free(bucket);
	Free(hash);
}

bool DHashSet(DHashT hash, const char *key, int klen, const char *value, int vlen)
{
	ASSERTOBJ(hash);
	assert(key && klen != 0);
	uint32_t                hashvalue = 0;
	int                     *counter = NULL;
	bool                    flag = false;
	struct DHashNode        **startptr = NULL;

	/*calculate for length of key and value, if they are string*/
	klen = klen < 0 ? (int)strlen(key) : klen;
	vlen = vlen < 0 ? (value ? (int)strlen(value) : 0) : vlen;
	
	assert(klen != 0);
	
	DHASH_SEARCH_BEGIN(flag, hash, key, klen, startptr, hashvalue, counter);

	if (likely(flag)) {
		x_printf(W, "this key `%.*s` is already exists", klen, key);
		flag = false;
		errno = EEXIST;
	} else {
		/*new insert*/
		struct DHashNode *node = NULL;
		/*allocate a new node, and initial it*/
		node = _DHashNodeAllocate(NULL, klen, vlen);

		if (likely(node)) {
			/*copy key and value to node*/
			memcpy(node->data, key, klen);

			if (value && vlen) {
				memcpy(node->data + klen, value, vlen);
			}

			/*store hash value to compare key*/
			node->hashv = hashvalue;
			/*insert the new node into bucket*/
			node->next = (*startptr);
			*startptr = node;
			/*increase counter of node*/
			ATOMIC_INC(counter);
			flag = true;
		}
	}

#if 0
	if (unlikely(!flag)) {
		/*destroy key and value ?*/
		if (hash->destroyk) {
			TRY { hash->destroyk(key, klen); } CATCH {} END;
		}

		if (hash->destroyv) {
			TRY { hash->destroyv(value, vlen); } CATCH {} END;
		}
	}
#endif

	DHASH_SEARCH_END();

	return flag;
}

bool DHashPut(DHashT hash, const char *key, int klen, const char *value, int vlen)
{
	ASSERTOBJ(hash);
	assert(key && klen != 0);
	uint32_t                hashvalue = 0;
	int                     *counter = NULL;
	bool                    flag = false;
	struct DHashNode        **startptr = NULL;

	/*calculate for length of key and value, if they are string*/
	klen = klen < 0 ? (int)strlen(key) : klen;
	vlen = vlen < 0 ? (value ? (int)strlen(value) : 0) : vlen;
	
	assert(klen != 0);
	
	DHASH_SEARCH_BEGIN(flag, hash, key, klen, startptr, hashvalue, counter);

	if (likely(flag)) {
		struct DHashNode *node = *startptr;

		/*destroy old value*/
		if (hash->destroyv) {
			TRY { hash->destroyv(node->data + node->klen, node->vlen); } CATCH {} END;
		}

		/*allocate new space for node,if it is necessary*/
		node = _DHashNodeAllocate(node, node->klen, vlen);

		if (likely(node)) {
			/*update old value*/
			if (value && vlen) {
				memcpy(node->data + node->klen, value, vlen);
			}

			/*update node in bucket*/
			*startptr = node;
		}
	} else {
		/*new insert*/
		struct DHashNode *node = NULL;
		/*allocate a new node, and initial it*/
		node = _DHashNodeAllocate(NULL, klen, vlen);

		if (likely(node)) {
			/*copy key and value to node*/
			memcpy(node->data, key, klen);

			if (value && vlen) {
				memcpy(node->data + klen, value, vlen);
			}

			/*store hash value to compare key*/
			node->hashv = hashvalue;
			/*insert the new node into bucket*/
			node->next = (*startptr);
			*startptr = node;
			/*increase counter of node*/
			ATOMIC_INC(counter);
			flag = true;
		}
	}

#if 0
	if (unlikely(!flag)) {
		/*destroy key and value ?*/
		if (hash->destroyk) {
			TRY { hash->destroyk(key, klen); } CATCH {} END;
		}

		if (hash->destroyv) {
			TRY { hash->destroyv(value, vlen); } CATCH {} END;
		}
	}
#endif
	DHASH_SEARCH_END();

	return flag;
}

bool DHashGet(DHashT hash, const char *key, int klen, char *value, unsigned *vlenptr)
{
	ASSERTOBJ(hash);
	assert(key && klen != 0);

	uint32_t                hashvalue = 0;
	int                     *counter = NULL;
	bool                    flag = false;
	struct DHashNode        **startptr = NULL;

	klen = klen < 0 ? (int)strlen(key) : klen;

	DHASH_SEARCH_BEGIN(flag, hash, key, klen, startptr, hashvalue, counter);

	if (likely(flag)) {
		/*store value to external memory*/
		if (value && vlenptr) {
			struct DHashNode        *node = (*startptr);
			int                     vlen = 0;
			vlen = MIN(*vlenptr, node->vlen);

			if (vlen > 0) {
				memcpy(value, node->data + node->klen, vlen);
			}

			SET_POINTER(vlenptr, vlen);
		}
	}

	DHASH_SEARCH_END();

	return flag;
}

bool DHashDel(DHashT hash, const char *key, int klen)
{
	ASSERTOBJ(hash);
	assert(key && klen != 0);

	uint32_t                hashvalue = 0;
	int                     *counter = NULL;
	bool                    flag = false;
	struct DHashNode        **startptr = NULL;

	klen = klen < 0 ? (int)strlen(key) : klen;

	DHASH_SEARCH_BEGIN(flag, hash, key, klen, startptr, hashvalue, counter);

	if (likely(flag)) {
		struct DHashNode *node = *startptr;

		/*destroy old key and value, if necessary*/
		if (hash->destroyk) {
			TRY { hash->destroyk(node->data, node->klen); } CATCH {} END;
		}

		if (hash->destroyv) {
			TRY { hash->destroyv(node->data + node->klen, node->vlen); } CATCH {} END;
		}

		/*free this node, update link of the bucket*/
		*startptr = node->next;
		free(node);
	}

	DHASH_SEARCH_END();
	return flag;
}

static bool _DHashSearch(struct DHashNode ***prevnodeptr,
	const char *key, int klen,
	uint32_t hashvalue, NodeCompareCB compare)
{
	bool flag = false;

	assert(compare);
	struct DHashNode **node = *prevnodeptr;

	TRY
	{
		while (likely(*node)) {
			if (((*node)->hashv == hashvalue) &&
#if 0
				((*node)->klen == klen) &&
#endif
				(compare((*node)->data, (*node)->klen, (void *)key, klen) == 0)) {
				flag = true;
				break;
			}

			/*step next,and store it's address to node*/
			node = &(*node)->next;
		}

		SET_POINTER(prevnodeptr, node);
	}
	CATCH
	{}
	END;

	return flag;
}

static inline struct DHashNode *_DHashNodeAllocate(struct DHashNode *old,
	uint32_t ksize, uint32_t vsize)
{
	uint32_t                need = 0;
	struct DHashNode        *node = NULL;

	need = ksize + vsize + offsetof(struct DHashNode, data);

	/*allocate memory, and update the lenght of key and value*/
	if ((old && (old->size < need)) || (old == NULL)) {
		node = realloc(old, need);

		if (likely(node)) {
			node->size = need;
			node->klen = ksize;
			node->vlen = vsize;
		} else {
			errno = ENOMEM;
			x_printf(W, "no more space, realloc() failed.");
		}
	} else {
		node = old;
		node->klen = ksize;
		node->vlen = vsize;
	}

	return node;
}

