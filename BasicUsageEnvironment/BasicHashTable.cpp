/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2017 Live Networks, Inc.  All rights reserved.
// Basic Hash Table implementation
// Implementation

#include "BasicHashTable.hh"
#include "strDup.hh"

#if defined(__WIN32__) || defined(_WIN32)
#else
#include <stddef.h>
#endif
#include <string.h>
#include <stdio.h>

// When there are this many entries per bucket, on average, rebuild
// the table to increase the number of buckets
#define REBUILD_MULTIPLIER 3

BasicHashTable::BasicHashTable(int keyType)
	: fBuckets(fStaticBuckets), fNumBuckets(SMALL_HASH_TABLE_SIZE),
	fNumEntries(0), fRebuildSize(SMALL_HASH_TABLE_SIZE*REBUILD_MULTIPLIER),
	fDownShift(28), fMask(0x3), fKeyType(keyType) {
	for (unsigned i = 0; i < SMALL_HASH_TABLE_SIZE; ++i) {
		fStaticBuckets[i] = NULL;
	}
}

BasicHashTable::~BasicHashTable() {
	// Free all the entries in the table:
	for (unsigned i = 0; i < fNumBuckets; ++i) {
		TableEntry* entry;
		while ((entry = fBuckets[i]) != NULL) {
			deleteEntry(i, entry);
		}
	}

	// Also free the bucket array, if it was dynamically allocated:
	if (fBuckets != fStaticBuckets) delete[] fBuckets; // 存在动态扩容后的数组
}
/*
向 BasicHashTable 中插入元素的过车大致如下：

1查找 BasicHashTable 中与要插入的键-值对的键匹配的元素 TableEntry。
2若找到，把该元素的旧的值保存在 oldValue 中。
3若没有找到，则通过 insertNewEntry(index, key) 创建一个 TableEntry 并加入到哈希桶中，oldValue 被赋值为 NULL。
4把要插入的键-值对的值保存进新创建或找到的 TableEntry 中。
5如果 BasicHashTable 中的元素个数超出 fRebuildSize 的大小，则对哈希桶扩容。
6返回元素的旧的值。

*/
void* BasicHashTable::Add(char const* key, void* value) {
	void* oldValue;
	unsigned index;
	// 根据 key 查找
	TableEntry* entry = lookupKey(key, index); 
	if (entry != NULL) {
		// There's already an item with this key
		// 哈希桶中已经存在相同的 key，并得到key所指的TableEntry
		oldValue = entry->value;
	}
	else {
		// There's no existing entry; create a new one:
		entry = insertNewEntry(index, key);
		oldValue = NULL;
	}
	entry->value = value;

	// If the table has become too large, rebuild it with more buckets:
	if (fNumEntries >= fRebuildSize) rebuild();

	return oldValue;
}

Boolean BasicHashTable::Remove(char const* key) {
	unsigned index;
	TableEntry* entry = lookupKey(key, index);
	if (entry == NULL) return False; // no such entry

	deleteEntry(index, entry);

	return True;
}
// 根据key 查找 TableEntry,然后返回 value
void* BasicHashTable::Lookup(char const* key) const {
	unsigned index;
	TableEntry* entry = lookupKey(key, index);
	if (entry == NULL) return NULL; // no such entry

	return entry->value;
}

unsigned BasicHashTable::numEntries() const {
	return fNumEntries;
}

BasicHashTable::Iterator::Iterator(BasicHashTable const& table)
	: fTable(table), fNextIndex(0), fNextEntry(NULL) {
}
// 遍历 HashTable
void* BasicHashTable::Iterator::next(char const*& key) {
	while (fNextEntry == NULL) {
		if (fNextIndex >= fTable.fNumBuckets) return NULL;

		fNextEntry = fTable.fBuckets[fNextIndex++];
	}

	BasicHashTable::TableEntry* entry = fNextEntry;
	fNextEntry = entry->fNext;

	key = entry->key;
	return entry->value;
}

////////// Implementation of HashTable creation functions //////////

HashTable* HashTable::create(int keyType) {
	return new BasicHashTable(keyType);
}

HashTable::Iterator* HashTable::Iterator::create(HashTable const& hashTable) {
	// "hashTable" is assumed to be a BasicHashTable
	return new BasicHashTable::Iterator((BasicHashTable const&)hashTable);
}

////////// Implementation of internal member functions //////////

BasicHashTable::TableEntry* BasicHashTable
::lookupKey(char const* key, unsigned& index) const {
	TableEntry* entry;
	index = hashIndexFromKey(key);
	// 根据 key 在哈希桶中查找，是否存在相同的 TableEntry，如果存在则停止循环，返回当前TableEntry指针
	for (entry = fBuckets[index]; entry != NULL; entry = entry->fNext) {
		if (keyMatches(key, entry->key)) break;
	}

	return entry;
}

Boolean BasicHashTable
::keyMatches(char const* key1, char const* key2) const {
	// The way we check the keys for a match depends upon their type:
	if (fKeyType == STRING_HASH_KEYS) {
		return (strcmp(key1, key2) == 0);
	}
	else if (fKeyType == ONE_WORD_HASH_KEYS) {
		return (key1 == key2);
	}
	else {
		unsigned* k1 = (unsigned*)key1;
		unsigned* k2 = (unsigned*)key2;

		for (int i = 0; i < fKeyType; ++i) {
			if (k1[i] != k2[i]) return False; // keys differ
		}
		return True;
	}
}

BasicHashTable::TableEntry* BasicHashTable
::insertNewEntry(unsigned index, char const* key) {
	TableEntry* entry = new TableEntry();
	entry->fNext = fBuckets[index]; // 获得在 哈希桶的 位置指针
	fBuckets[index] = entry;

	++fNumEntries; // 增加计数器
	assignKey(entry, key); // 把key赋值给 TableEntry对象的key 

	return entry;
}

void BasicHashTable::assignKey(TableEntry* entry, char const* key) {
	// The way we assign the key depends upon its type:
	if (fKeyType == STRING_HASH_KEYS) {
		entry->key = strDup(key);
	}
	else if (fKeyType == ONE_WORD_HASH_KEYS) {
		entry->key = key;
	}
	else if (fKeyType > 0) {
		unsigned* keyFrom = (unsigned*)key;
		unsigned* keyTo = new unsigned[fKeyType]; // 数组
		for (int i = 0; i < fKeyType; ++i) keyTo[i] = keyFrom[i]; // 复制[0,fKeyType]的key值
		/*
		对比 keyMatches() 和 assignKey() 函数的实现，不难发现，当 HashTable 类型 fKeyType 大于0，且不是 ONE_WORD_HASH_KEYS 时，要求作为哈希表中键值对的键的字符串的长度固定为 (sizeof(unsigned) * fKeyType) 个字节。
		*/
		entry->key = (char const*)keyTo;
	}
}

/*
通过二级指针，遍历链表一趟，就将元素移除出去了。记得这是这中场景下 Linus 大神鼓励的一种写法。从链表中删除一个元素，用好几个临时变量，或者加许多判断的方法，都弱爆了。
*/
void BasicHashTable::deleteEntry(unsigned index, TableEntry* entry) {
	TableEntry** ep = &fBuckets[index];

	Boolean foundIt = False;
	while (*ep != NULL) {
		if (*ep == entry) {
			foundIt = True;
			*ep = entry->fNext;
			break;
		}
		ep = &((*ep)->fNext);
	}

	if (!foundIt) { // shouldn't happen
#ifdef DEBUG
		fprintf(stderr, "BasicHashTable[%p]::deleteEntry(%d,%p): internal error - not found (first entry %p", this, index, entry, fBuckets[index]);
		if (fBuckets[index] != NULL) fprintf(stderr, ", next entry %p", fBuckets[index]->fNext);
		fprintf(stderr, ")\n");
#endif
	}

	--fNumEntries;
	deleteKey(entry); // 释放 TableEntry中的 key
	delete entry;
}

void BasicHashTable::deleteKey(TableEntry* entry) {
	// The way we delete the key depends upon its type:
	if (fKeyType == ONE_WORD_HASH_KEYS) {
		entry->key = NULL;
	}
	else {
		delete[](char*)entry->key;
		entry->key = NULL;
	}
}

/*

为 fBuckets 分配一块新的内存，容量为原来的4倍。
适当更新 fNumBuckets，fRebuildSize，fDownShift 和 fMask 等。
将老的 fBuckets 中的元素，依据元素的 key 和新的哈希桶的容量，搬到新的 fBuckets 中。
根据需要释放老的 fBuckets 的内存。

*/
void BasicHashTable::rebuild() {
	// Remember the existing table size:
	unsigned oldSize = fNumBuckets;
	TableEntry** oldBuckets = fBuckets;

	// Create the new sized table:
	fNumBuckets *= 4; // 扩容为原来的 4 倍
	fBuckets = new TableEntry*[fNumBuckets];
	for (unsigned i = 0; i < fNumBuckets; ++i) {
		fBuckets[i] = NULL;
	}
	fRebuildSize *= 4;
	fDownShift -= 2;
	fMask = (fMask << 2) | 0x3;
	
	// Rehash the existing entries into the new table:
	for (TableEntry** oldChainPtr = oldBuckets; oldSize > 0;
		--oldSize, ++oldChainPtr) {
		for (TableEntry* hPtr = *oldChainPtr; hPtr != NULL;
			hPtr = *oldChainPtr) {
			*oldChainPtr = hPtr->fNext;

			unsigned index = hashIndexFromKey(hPtr->key);

			hPtr->fNext = fBuckets[index];
			fBuckets[index] = hPtr;
		}
	}

	// Free the old bucket array, if it was dynamically allocated:
	if (oldBuckets != fStaticBuckets) delete[] oldBuckets;
}

// 根据不同的keyType 计算哈希值，然后映射到哈系桶，得到索引
unsigned BasicHashTable::hashIndexFromKey(char const* key) const {
	unsigned result = 0;

	if (fKeyType == STRING_HASH_KEYS) {
		while (1) {
			char c = *key++;
			if (c == 0) break;
			result += (result << 3) + (unsigned)c;
		}
		result &= fMask;
	}
	else if (fKeyType == ONE_WORD_HASH_KEYS) {
		result = randomIndex((uintptr_t)key);
	}
	else {
		unsigned* k = (unsigned*)key;
		uintptr_t sum = 0;
		for (int i = 0; i < fKeyType; ++i) {
			sum += k[i];
		}
		result = randomIndex(sum);
	}

	return result;
}
