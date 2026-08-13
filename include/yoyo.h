#pragma once

#include <GMLScriptEnv/yoyo.h>


#pragma pack(push, 1)
// TODO: move these over to roa-mod-loader

// GameMaker functions that start with F_ share this format
typedef RValue& (*PFUNC_F_CALL)(
	_Out_ RValue* out,
	_In_ CInstance* self,
	_In_ CInstance* other,
	_In_ int argCount,
	_In_ RValue* args
	);

struct DynamicArrayOfRValue
{
	int length;
	RValue* arr;
};

struct RefDynamicArrayOfRValue
{
	int	refcount;
	DynamicArrayOfRValue* pArray;
	void* pOwner;
	int visited;
	int length;
};

template<typename T>
struct Hash
{
	int field0_0x0;
	int field1_0x4;
	int field2_0x8;
};

template<typename T>
struct CObjectHashNode
{
	CObjectHashNode* m_prev;
	CObjectHashNode* m_next;
	uint32_t m_key;
	T* m_object;
};

template<typename T>
struct CObjectHashBucket
{
	CObjectHashNode<T>* m_head;
	CObjectHashNode<T>* m_tail;
};

template<typename T>
struct CObjectHashMap
{
	CObjectHashBucket<T>* m_buckets;
	uint32_t m_mask;
	uint32_t m_count;

	T* find_object(uint32_t key) const
	{
		if (!m_buckets)
		{
			return nullptr;
		}
		uint32_t index = key & m_mask;
		for (auto node = m_buckets[index].m_head; node; node = node->m_next)
		{
			if (node->m_key == key)
			{
				return node->m_object;
			}
		}
		return nullptr;
	}
};

using ObjectsHash = CObjectHashMap<CObjectGM>;

struct tagIConsole
{
	void** vtable[4];
};
#pragma pack(pop)