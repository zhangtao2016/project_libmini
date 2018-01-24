//
//  adt_proto.c
//
//
//  Created by 周凯 on 15/9/1.
//
//

#include "adt/adt_proto.h"
#include "except/exception.h"
/*
 * 常量定义
 */

/*hash每次计算多少位*/
#define HASHWORDBITS            (32)
/*哈希桶的最小大小(质数) */
#define HASH_BASE_MIN_PRIME     (31U)
/**哈希桶的最大大小(质数) */
#define HASH_BASE_MAX_PRIME     (4294967279U)

/**
 * key键默认比较函数，作为整数处理
 */
int HashCompareDefaultCB(const char *data, int datasize, void *user, int usrsize)
{
//	assert(data && user && datasize && usrsize);
//	return ((intptr_t)data < (intptr_t)user) ? -1 : (((intptr_t)data == (intptr_t)user) ? 0 : 1);
	if (datasize < 0 || usrsize < 0) {
		return strcmp(data, user);
	} else if (datasize == usrsize) {
		long long d = 0;
		long long u = 0;
		if (datasize < 2) {
			d = ((char *)data)[0];
			u = ((char *)user)[0];
		} else if (datasize < 4) {
			d = ((short *)data)[0];
			u = ((short *)user)[0];
		} else if (datasize < 8) {
			d = ((int *)data)[0];
			u = ((int *)user)[0];
		} else {
			d = ((long long *)data)[0];
			u = ((long long *)user)[0];
		}
		
		return d < u ? -1 : (d == u ? 0 : 1);
		
	} else if (datasize < usrsize) {
		return -1;
	}
	
	return 1;
}

uint32_t HashDefaultCB(const char *key, int dataLen)
{
	uint64_t d = 0;
	if (dataLen < 2) {
		d = ((uint8_t *)key)[0];
	} else if (dataLen < 4) {
		d = ((uint16_t *)key)[0];
	} else if (dataLen < 8) {
		d = ((uint32_t *)key)[0];
	} else {
		d = ((uint64_t *)key)[0];
	}
	return (uint32_t)(d >> 3);
}

/*
 * 哈希值计算包裹函数
 * @param data 被计算的键，可以为NULL
 * @param dataLen 被计算的键的长度，如果为负数，则将data看作字符串
 * @return 0 ~ 0xffffffffU的哈希值
 */
uint32_t HashFlower(const char *key, int keyLen)
{
	const uint8_t   *k = (const uint8_t *)key;
	uint32_t        h = 0;
	long            i = 0;

	if (!k) {
		return 0;
	}

	if (keyLen < 0) {
		keyLen = (int)strlen((const char *)k);
	}

	for (i = 0, h = 0; i < keyLen; i++) {
		h *= 16777619;
		h ^= k[i];
	}

	return h;
}

uint32_t HashTime33(const char *key, int keyLen)
{
	const uint8_t   *k = (const uint8_t *)key;
	uint32_t        h = 0;
	long            i = 0;

	if (!k) {
		return 0;
	}

	if (keyLen < 0) {
		keyLen = (int)strlen((const char *)k);
	}

	for (i = 0; i < keyLen; i++) {
		h = h * 33 + k[i];
	}

	return h;
}

uint32_t HashIgnoreCaseTime33(const char *key, int keyLen)
{
	const uint8_t   *k = (const uint8_t *)key;
	uint32_t        h = 0;
	long            i = 0;

	if (!k) {
		return 0;
	}

	if (keyLen < 0) {
		keyLen = (int)strlen((const char *)k);
	}

	for (i = 0; i < keyLen; i++) {
		h = h * 33 + toupper(k[i]);
	}

	return h;
}

uint32_t HashReduceBit(const char *key, int keyLen)
{
	const uint8_t   *k = (const uint8_t *)key;
	long            i = 0;
	uint32_t        h = 0;

	if (!k) {
		return ~((uint32_t)0);
	}

	if (keyLen < 0) {
		keyLen = (int)strlen((const char *)k);
	}

	for (h = (uint32_t)keyLen, i = 0; i < keyLen; i++) {
		h = (h << 9) | (h >> (sizeof(h) * CHAR_BIT - 9));
		h += k[i];
	}

	return h != 0 ? h : ~((uint32_t)0);
}

uint32_t HashPJW(const char *key, int keyLen)
{
	const uint8_t   *k = (const uint8_t *)key;
	uint32_t        h = 0;
	uint32_t        g = 0;
	long            i = 0;

	if (!k) {
		return 0;
	}

	if (keyLen < 0) {
		keyLen = (int)strlen((const char *)k);
	}

	for (h = (uint32_t)keyLen, i = 0; i < keyLen; i++) {
		h <<= 4;
		h += k[i];
		g = h & ((uint32_t)0xf << (HASHWORDBITS - 4));

		if (g != 0) {
			h ^= g >> (HASHWORDBITS - 8);
			h ^= g;
		}
	}

	return h;
}

/**
 * 判断给定的数值是否为质数
 */
bool IsPrime(unsigned candidate)
{
	/* No even number and none less than 10 will be passed here.  */
	unsigned        divn = 3;
	unsigned        sq = divn * divn;

	while (sq < candidate && candidate % divn != 0) {
		++divn;
		sq += 4 * divn;
		++divn;
	}

	return candidate % divn != 0;
}

/**
 * 返回给定数值对应的下一质数
 */
int NextPrime(unsigned seed)
{
	/* Make it definitely odd.  */
	seed |= 1;

	while (!IsPrime(seed)) {
		seed += 2;
	}

	return seed;
}

/**
 * 计算哈希表的长度
 */
int CalculateTableSize(unsigned hint)
{
	if (hint <= HASH_BASE_MIN_PRIME) {
		return HASH_BASE_MIN_PRIME;
	}

	if (hint >= HASH_BASE_MAX_PRIME) {
		return HASH_BASE_MAX_PRIME;
	}

	return NextPrime((unsigned)hint);
}

