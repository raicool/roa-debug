#pragma once

#include <GMLScriptEnv/yoyo.h>


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

class CEvent;
class CObjectGM
{
	char* m_Name;
	int param_0x04;
	CHashMap<int, CObjectGM*, 2> param_0x08;
	CHashMap<uint64_t, CEvent*, 3> param_0x0c;
};

template <typename T>
class Hash
{
	void* ptr_0x00;
	unsigned int param_0x04;
	int param_0x08;
};

struct tagIConsole
{
	void** vtable[4];
};