//
//  dhash.h
//  supex
//
//  Created by 周凯 on 15/12/22.
//  Copyright © 2015年 zhoukai. All rights reserved.
//

#ifndef dhash_h
#define dhash_h

#include <stdio.h>
#include "../adt_proto.h"

__BEGIN_DECLS

typedef struct DHash *DHashT;

struct DHash
{
	int             magic;
	int             nodes;
	void            *bucket;
	HashCB          hashk;
	NodeCompareCB   comparek;
	NodeDestroyCB   destroyk;
	NodeDestroyCB   destroyv;
};

/**
 * 创建散列表，当不传递值（和值长度）时，可以实现集合
 * key的长度不能为零，即空字符串不能作为键
 */
DHashT DHashCreate(int size, HashCB hash, NodeCompareCB compare);

void DHashDestroy(DHashT *hash);

/**
 * 新增一份键值副本，如果键存在，则失败
 * @param key
 * @param klen 不能为0，当<0时，key被当作字符串处理
 * @param value
 * @param vlen
 */
bool DHashSet(DHashT hash, const char *key, int klen, const char *value, int vlen);

/**新增或更新一份键值副本*/
bool DHashPut(DHashT hash, const char *key, int klen, const char *value, int vlen);

/**获取一份值副本*/
bool DHashGet(DHashT hash, const char *key, int klen, char *value, unsigned *vlenptr);

/**删除一份键值副本*/
bool DHashDel(DHashT hash, const char *key, int klen);

__END_DECLS
#endif	/* dhash_h */

