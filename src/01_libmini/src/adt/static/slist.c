#include "adt/static/slist.h"
#include "slog/slog.h"
#include "except/exception.h"
/* --------------                       */

typedef struct SListNode *SListNodeT;

struct SListNode
{
	int     magic;
	int     previdx;
	int     nextidx;
	int     size;
	char    data[1];
};

/* ----------------                 */

#define calculate_node_size(size) \
	((int)(offsetof(struct SListNode, data) + ADJUST_SIZE((size), sizeof(long))))

#define calculate_total_size(total, size) \
	((long)((total) * calculate_node_size((size)) + sizeof(struct SList)))

#define calculate_node_ptr(list, i)		       \
	(assert((i) < (list)->totalnodes && (i) > -1), \
	(SListNodeT)((((char *)(list)) + (list)->buffoffset) + (i) * calculate_node_size((list)->datasize)))

static inline int calculate_node_idx(SListT list, const char *nodedata)
{
	char    *startp = NULL;
	char    *endp = NULL;
	char    *node = NULL;
	int     nodesize = 0;
	int     i = 0;
	int     j = 0;

	ASSERTOBJ(list);

	nodesize = calculate_node_size(list->datasize);
	startp = ((char *)list) + list->buffoffset;
	endp = ((char *)list) + list->totalsize - nodesize;
	node = (char *)container_of(nodedata, struct SListNode, data);

	assert(startp <= node && node <= endp);

	i = (int)((node - startp) / nodesize);
	j = (int)((node - startp) % nodesize);

	assert(j == 0);

	return i;
}

/* ----------------                 */

/**删除指定节点下标，返回删除的节点下标*/
static inline int _DelNode(SListT list, int delidx);

/**在指定节点下标和下一个节点之间插入一个新的节点，并返回下标，如果指定节点为－1则更新首尾节点下标*/
static inline int _InsertNextNode(SListT list, int curidx);

/**在指定节点下标和上一个节点之间插入一个新的节点，并返回下标，如果指定节点为－1则更新首尾节点下标*/
static inline int _InsertPrevNode(SListT list, int curidx);

/* ----------------                 */
static inline int _NextDelNode(SListT list, int *idx);

static inline int _PrevDelNode(SListT list, int *idx);

/* ----------------                 */

void SListLock(SListT list)
{
	ASSERTOBJ(list);

	AO_ThreadSpinLock(&list->lock);
}

void SListUnlock(SListT list)
{
	ASSERTOBJ(list);

	AO_ThreadSpinUnlock(&list->lock);
}

void SListAddRefs(SListT list)
{
	ASSERTOBJ(list);

	ATOMIC_INC(&list->refs);
}

int SListDecRefs(SListT list)
{
	ASSERTOBJ(list);

	return ATOMIC_SUB_F(&list->refs, 1);
}

int SListGetRefs(SListT list)
{
	ASSERTOBJ(list);

	return ATOMIC_GET(&list->refs);
}

/* ----------------                 */

long SListCalculateNodeSize(int datasize)
{
	return calculate_node_size(datasize);
}

long SListCalculateSize(int nodetotal, int datasize)
{
	return calculate_total_size(nodetotal, datasize);
}

/* ----------------                 */

SListT SListInit(char *buff, long size, int datasize, bool shared)
{
	SListT          list = NULL;
	SListNodeT      node = NULL;
	int             i = 0;
	int             nodesize = 0;
	long            totalsize = 0;

	assert(buff);

	return_val_if_fail(size > 0 && datasize > 0, NULL);

	/*至少要能容纳一个节点*/
	if (unlikely(calculate_total_size(1, datasize) > size)) {
		return NULL;
	}

	nodesize = calculate_node_size(datasize);

	list = (SListT)buff;
	memset(list, 0, sizeof(*list));

	AO_ThreadSpinLockInit(&list->lock, shared);

	list->nodes = 0;
	list->headidx = -1;
	list->tailidx = -1;
	list->unusedidx = 0;

	list->datasize = datasize;
	list->totalnodes = (int)CEIL_TIMES(size - sizeof(*list), nodesize) - 1;

	totalsize = calculate_total_size(list->totalnodes, datasize);

	if (likely((size - totalsize) == nodesize)) {
		list->totalnodes += 1;
	}

	list->buffoffset = sizeof(*list);
	list->totalsize = size;

	for (i = 0; i < list->totalnodes - 1; i++) {
		node = calculate_node_ptr(list, i);
		node->nextidx = i + 1;
		node->previdx = i - 1;
		node->size = -1;
		REFOBJ(node);
	}

	node = calculate_node_ptr(list, i);
	node->nextidx = -1;
	node->previdx = i - 1;
	REFOBJ(node);

	REFOBJ(list);

	return list;
}

bool SListAddHead(SListT list, char *buff, int size, int *effectsize, void **inptr)
{
	int             newheadidx = -1;
	SListNodeT      newheadnode = NULL;
	bool            ret = false;

	ASSERTOBJ(list);
	assert(buff && size > -1);
	/*是否可以插入一个长度为0或者大于节点容量的数据*/
#if 1
	return_val_if_fail(size > 0 && size < list->datasize + 1, false);
#endif

	AO_ThreadSpinLock(&list->lock);

	newheadidx = _InsertPrevNode(list, list->headidx);

	if (likely(newheadidx > -1)) {
		newheadnode = calculate_node_ptr(list, newheadidx);
		ASSERTOBJ(newheadnode);

		newheadnode->size = MIN(size, list->datasize);

		if (likely(newheadnode->size > 0)) {
			memcpy(newheadnode->data, buff, newheadnode->size);
		}

		SET_POINTER(inptr, newheadnode->data);
		SET_POINTER(effectsize, newheadnode->size);

		ret = true;
	}

	AO_ThreadSpinUnlock(&list->lock);

	return ret;
}

bool SListCheckHead(SListT list, char *buff, int size, int *effectsize, void **inptr)
{
	ASSERTOBJ(list);

#if 0
	return_val_if_fail(buff && size > 0, false);
#endif

	AO_ThreadSpinLock(&list->lock);

	if (unlikely(list->headidx < 0)) {
		AO_ThreadSpinUnlock(&list->lock);
		return false;
	}

	if (likely(buff && (size > 0))) {
		SListNodeT node = calculate_node_ptr(list, list->headidx);
		ASSERTOBJ(node);
		int len = MIN(node->size, size);

		if (likely(len > 0)) {
			memcpy(buff, node->data, len);
		}

		SET_POINTER(effectsize, len);
		SET_POINTER(inptr, node->data);
	} else {
		SListNodeT node = calculate_node_ptr(list, list->tailidx);
		ASSERTOBJ(node);
		SET_POINTER(effectsize, node->size);
	}

	AO_ThreadSpinUnlock(&list->lock);

	return true;
}

bool SListAddTail(SListT list, char *buff, int size, int *effectsize, void **inptr)
{
	int             newtailidx = -1;
	SListNodeT      newtailnode = NULL;
	bool            ret = false;

	ASSERTOBJ(list);
	assert(buff && size > -1);
	/*是否可以插入一个长度为0或者大于节点容量的数据*/
#if 1
	return_val_if_fail(size > 0 && size < list->datasize + 1, false);
#endif

	AO_ThreadSpinLock(&list->lock);

	newtailidx = _InsertNextNode(list, list->tailidx);

	if (likely(newtailidx > -1)) {
		newtailnode = calculate_node_ptr(list, newtailidx);
		ASSERTOBJ(newtailnode);

		newtailnode->size = MIN(size, list->datasize);

		if (likely(newtailnode->size > 0)) {
			memcpy(newtailnode->data, buff, newtailnode->size);
		}

		SET_POINTER(inptr, newtailnode->data);
		SET_POINTER(effectsize, newtailnode->size);

		ret = true;
	}

	AO_ThreadSpinUnlock(&list->lock);

	return ret;
}

bool SListCheckTail(SListT list, char *buff, int size, int *effectsize, void **inptr)
{
	ASSERTOBJ(list);

#if 0
	return_val_if_fail(buff && size > 0, false);
#endif

	AO_ThreadSpinLock(&list->lock);

	if (unlikely(list->tailidx < 0)) {
		AO_ThreadSpinUnlock(&list->lock);
		return false;
	}

	if (likely(buff && (size > 0))) {
		SListNodeT node = calculate_node_ptr(list, list->tailidx);
		ASSERTOBJ(node);
		int len = MIN(node->size, size);

		if (likely(len > 0)) {
			memcpy(buff, node->data, len);
		}

		SET_POINTER(effectsize, len);
		SET_POINTER(inptr, node->data);
	} else {
		SListNodeT node = calculate_node_ptr(list, list->tailidx);
		ASSERTOBJ(node);
		SET_POINTER(effectsize, node->size);
	}

	AO_ThreadSpinUnlock(&list->lock);

	return true;
}

void SListReverseTravel(SListT list, NodeTravelCB travel, void *user)
{
	int previdx = -1;

	assert(travel);

	ASSERTOBJ(list);

	TRY
	{
		AO_ThreadSpinLock(&list->lock);

		previdx = list->tailidx;

		while (previdx > -1) {
			SListNodeT node = calculate_node_ptr(list, previdx);
			ASSERTOBJ(node);

			if (!travel(node->data, node->size, user)) {
				break;
			}
			
			previdx = node->previdx;
		}
	}
	FINALLY
	{
		AO_ThreadSpinUnlock(&list->lock);
	}
	END;
}

void SListTravel(SListT list, NodeTravelCB travel, void *user)
{
	int nextidx = -1;

	assert(travel);

	ASSERTOBJ(list);

	TRY
	{
		AO_ThreadSpinLock(&list->lock);

		nextidx = list->headidx;

		while (nextidx > -1) {
			SListNodeT node = calculate_node_ptr(list, nextidx);
			ASSERTOBJ(node);

			if (!travel(node->data, node->size, user)) {
				break;
			}

			nextidx = node->nextidx;
		}
	}
	FINALLY
	{
		AO_ThreadSpinUnlock(&list->lock);
	}
	END;
}

void SListTravelDel(SListT list, NodeCompareCB compare, void *user, int usrsize)
{
	int previdx = -1;

	assert(compare);

	ASSERTOBJ(list);

	TRY
	{
		AO_ThreadSpinLock(&list->lock);

		previdx = list->headidx;

		while (previdx > -1) {
			SListNodeT node = calculate_node_ptr(list, previdx);
			ASSERTOBJ(node);

			if (compare(node->data, node->size, user, usrsize) == 0) {
				_DelNode(list, previdx);
			}

			previdx = node->nextidx;
		}
	}
	FINALLY
	{
		AO_ThreadSpinUnlock(&list->lock);
	}
	END;
}

const char *SListLookup(SListT list, NodeCompareCB compare, void *user, int usrsize, int *datasize)
{
	int             previdx = -1;
	const char      *data = NULL;

	assert(compare);

	ASSERTOBJ(list);

	TRY
	{
		AO_ThreadSpinLock(&list->lock);

		previdx = list->tailidx;

		while (previdx > -1) {
			SListNodeT node = calculate_node_ptr(list, previdx);
			ASSERTOBJ(node);

			if (compare(node->data, node->size, user, usrsize) == 0) {
				data = &node->data[0];
				SET_POINTER(datasize, node->size);
				break;
			}

			previdx = node->previdx;
		}
	}
	FINALLY
	{
		AO_ThreadSpinUnlock(&list->lock);
	}
	END;

	return data;
}

void SListDelData(SListT list, const char *nodedata)
{
	int delidx = -1;

	ASSERTOBJ(list);

	return_if_fail(nodedata);

	AO_ThreadSpinLock(&list->lock);

	delidx = calculate_node_idx(list, nodedata);
	_DelNode(list, delidx);

	AO_ThreadSpinUnlock(&list->lock);
}

bool SListDelHead(SListT list, char *buff, int size, int *effectsize)
{
	int delidx = -1;

	ASSERTOBJ(list);

#if 0
	return_val_if_fail(buff && size > 0, false);
#endif

	AO_ThreadSpinLock(&list->lock);

	if (unlikely(list->headidx < 0)) {
		AO_ThreadSpinUnlock(&list->lock);
		return false;
	}

	delidx = _DelNode(list, list->headidx);

	if (likely(buff && (size > 0))) {
		SListNodeT delnode = calculate_node_ptr(list, delidx);
		ASSERTOBJ(delnode);
		int len = MIN(delnode->size, size);

		if (likely(len > 0)) {
			memcpy(buff, delnode->data, len);
		}

		SET_POINTER(effectsize, len);
	} else {
		SET_POINTER(effectsize, 0);
	}

	AO_ThreadSpinUnlock(&list->lock);

	return true;
}

bool SListDelTail(SListT list, char *buff, int size, int *effectsize)
{
	int delidx = -1;

	ASSERTOBJ(list);

#if 0
	return_val_if_fail(buff && size > 0, false);
#endif

	AO_ThreadSpinLock(&list->lock);

	if (unlikely(list->tailidx < 0)) {
		AO_ThreadSpinUnlock(&list->lock);
		return false;
	}

	delidx = _DelNode(list, list->tailidx);

	if (likely(buff && (size > 0))) {
		SListNodeT delnode = calculate_node_ptr(list, delidx);
		ASSERTOBJ(delnode);
		int len = MIN(delnode->size, size);

		if (likely(len > 0)) {
			memcpy(buff, delnode->data, len);
		}

		SET_POINTER(effectsize, len);
	} else {
		SET_POINTER(effectsize, 0);
	}

	AO_ThreadSpinUnlock(&list->lock);

	return true;
}

/**
 * 通过前驱节点删除给出位置的节点，改变后驱节点
 */
static inline int _PrevDelNode(SListT list, int *idx)
{
	int             delidx = *idx;
	SListNodeT      delnode = calculate_node_ptr(list, delidx);

	ASSERTOBJ(delnode);

	/*剥离节点*/
	if (unlikely(delnode->nextidx != -1)) {
		/*非尾节点*/
		SListNodeT nextnode = calculate_node_ptr(list, delnode->nextidx);
		ASSERTOBJ(nextnode);
		nextnode->previdx = delnode->previdx;
	} else {
		/*尾节点*/
		assert(list->tailidx == delidx);
		list->tailidx = delnode->previdx;
	}

	*idx = delnode->nextidx;

	/*回收节点*/
	delnode->nextidx = list->unusedidx;
	delnode->previdx = -1;
	list->unusedidx = delidx;

	ATOMIC_DEC(&list->nodes);

	return delidx;
}

/**
 * 通过后驱节点删除给出位置的节点，改变前导节点
 */
static inline int _NextDelNode(SListT list, int *idx)
{
	int             delidx = *idx;
	SListNodeT      delnode = calculate_node_ptr(list, delidx);

	ASSERTOBJ(delnode);

	/*剥离节点*/
	if (unlikely(delnode->previdx != -1)) {
		/*非头节点*/
		SListNodeT prevnode = calculate_node_ptr(list, delnode->previdx);
		ASSERTOBJ(prevnode);
		prevnode->nextidx = delnode->nextidx;
	} else {
		/*头节点*/
		assert(list->headidx == delidx);
		list->headidx = delnode->nextidx;
	}

	*idx = delnode->previdx;

	/*回收节点*/
	delnode->nextidx = list->unusedidx;
	delnode->previdx = -1;
	list->unusedidx = delidx;

	ATOMIC_DEC(&list->nodes);

	return delidx;
}

/*
 * 删除指定节点
 */
static inline int _DelNode(SListT list, int delidx)
{
	SListNodeT delnode = NULL;

	ASSERTOBJ(list);

	delnode = calculate_node_ptr(list, delidx);
	ASSERTOBJ(delnode);

	if (unlikely(delnode->nextidx != -1)) {
		SListNodeT nextnode = NULL;

		nextnode = calculate_node_ptr(list, delnode->nextidx);
		ASSERTOBJ(nextnode);
		nextnode->previdx = delnode->previdx;
	} else {
		/*尾部节点更新*/
		assert(list->tailidx == delidx);
		list->tailidx = delnode->previdx;
	}

	if (likely(delnode->previdx != -1)) {
		SListNodeT prevnode = NULL;

		prevnode = calculate_node_ptr(list, delnode->previdx);
		ASSERTOBJ(prevnode);
		prevnode->nextidx = delnode->nextidx;
	} else {
		/*头部节点更新*/
		assert(list->headidx == delidx);
		list->headidx = delnode->nextidx;
	}

	/*回收节点*/
	delnode->nextidx = list->unusedidx;
	delnode->previdx = -1;
	list->unusedidx = delidx;

	ATOMIC_DEC(&list->nodes);

	return delidx;
}

static inline int _InsertNextNode(SListT list, int curidx)
{
	int             newidx = -1;
	SListNodeT      newnode = NULL;

	ASSERTOBJ(list);

	if (unlikely(list->unusedidx < 0)) {
		x_printf(D, "no more space");
		return -1;
	}

	/*获取可用节点*/
	newidx = list->unusedidx;
	newnode = calculate_node_ptr(list, newidx);
	ASSERTOBJ(newnode);
	list->unusedidx = newnode->nextidx;

	if (unlikely(curidx == -1)) {
		list->tailidx = list->headidx = newidx;
		newnode->nextidx = newnode->previdx = -1;
	} else {
		int             nextidx = -1;
		SListNodeT      curnode = NULL;

		curnode = calculate_node_ptr(list, curidx);
		ASSERTOBJ(newnode);

		nextidx = curnode->nextidx;

		newnode->nextidx = nextidx;
		newnode->previdx = curidx;
		curnode->nextidx = newidx;

		if (unlikely(nextidx == -1)) {
			/*尾节点*/
			list->tailidx = newidx;
		} else {
			SListNodeT nextnode = NULL;
			nextnode = calculate_node_ptr(list, nextidx);
			ASSERTOBJ(nextnode);

			nextnode->previdx = newidx;
		}
	}

	ATOMIC_INC(&list->nodes);

	return newidx;
}

static inline int _InsertPrevNode(SListT list, int curidx)
{
	int             newidx = -1;
	SListNodeT      newnode = NULL;

	ASSERTOBJ(list);

	if (unlikely(list->unusedidx < 0)) {
		x_printf(D, "no more space");
		return -1;
	}

	/*获取可用节点*/
	newidx = list->unusedidx;
	newnode = calculate_node_ptr(list, newidx);
	ASSERTOBJ(newnode);
	list->unusedidx = newnode->nextidx;

	if (unlikely(curidx == -1)) {
		list->tailidx = list->headidx = newidx;
		newnode->nextidx = newnode->previdx = -1;
	} else {
		int             previdx = -1;
		SListNodeT      curnode = NULL;

		curnode = calculate_node_ptr(list, curidx);
		ASSERTOBJ(curnode);

		previdx = curnode->previdx;

		newnode->previdx = previdx;
		newnode->nextidx = curidx;
		curnode->previdx = newidx;

		if (likely(previdx == -1)) {
			/*头节点*/
			list->headidx = newidx;
		} else {
			SListNodeT prevnode = NULL;
			prevnode = calculate_node_ptr(list, previdx);
			ASSERTOBJ(prevnode);

			prevnode->nextidx = newidx;
		}
	}

	ATOMIC_INC(&list->nodes);

	return newidx;
}

