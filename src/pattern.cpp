#include "pattern.h"

#include "yoyo.h"
#include "utils.h"

#include <loader/log.h>

#include <vector>

PFUNC_F_CALL F_VMGetCallStack = nullptr;

ObjectsHash** g_ObjectHash = nullptr;
int** g_RngIndex = nullptr;
int** state = nullptr;

void pattern_find_object_hash()
{
	std::vector<uint8_t> pattern
	{
		0xa1, '?', '?', '?', '?',
		0x8b, 0x54, 0x24, '?',
		0x8b, 0x48, '?',
		0x8b, 0x00,
		0x23, 0xca,
		0x8b, 0x04, '?',
		0x85, 0xc0,
		0x74, 0x0c,
		0x39, 0x50, '?',
		0x74, '?'
	};

	uint32_t ptr_ObjectHash = (uint32_t)loader_search_memory(pattern);

	if (ptr_ObjectHash)
	{
		uint32_t real_ptr = *(uint32_t*)INCREMENT_POINTER(ptr_ObjectHash, 1);

		g_ObjectHash = (ObjectsHash**)real_ptr;
		loader_log_trace("g_ObjectHash = 0x{:08x}", (uint32_t)g_ObjectHash);
		CObjectGM* obj = (*g_ObjectHash)->find_object(0);
		if (obj)
		{
			loader_log_info("succeed {}", (void*)obj);
			loader_log_info("obj->m_Name = {}", obj->m_Name);
		}
		else
		{
			loader_log_info("fail {}", (void*)obj);
		}
	}
	else
	{
		loader_log_error("roa-debug: could not find Object_Find()");
	}
}

void pattern_find_rng_index()
{
	std::vector<uint8_t> pattern_YYRandom =
	{
		0x51,
		0x56,
		0x8b, 0x35, '?', '?', '?', '?',
		0x57,
		0x8b, 0x0c, 0xb5, '?', '?', '?', '?',
		0x8d, 0x46, 0xfd,
		0x83, 0xe0, 0x0f,
		0x8d, 0x3c, 0x09,
		0x8b, 0x04, 0x85, '?', '?', '?', '?',
		0x33, 0xf8,
		0xc1, 0xe7, 0x0f,
		0x33, 0xf8,
		0x8d, 0x46, 0xf9,
		0x83, 0xe0, 0x0f,
		0x33, 0xf9,
		0x8b, 0x0c, 0x85, '?', '?', '?', '?',
	};

	uint32_t ptr_YYRandom = (uint32_t)loader_search_memory(pattern_YYRandom);

	if (ptr_YYRandom)
	{
		g_RngIndex = (int**)INCREMENT_POINTER(ptr_YYRandom, 4);
		state = (int**)INCREMENT_POINTER(ptr_YYRandom, 12);
		loader_log_trace("g_RngIndex = 0x{:08x}", (uint32_t)g_RngIndex);
		loader_log_trace("state = 0x{:08x}", (uint32_t)state);
	}
	else
	{
		loader_log_error("roa-debug: could not find YYRandom()");
	}
}

void pattern_find_VMGetCallStack()
{
	std::vector<uint8_t> pattern
	{
		0x55,
		0x8d, 0xac, 0x24, 0xfc, 0xfe, 0xff, 0xff,
		0x81, 0xec, 0x04, 0x01, 0x00, 0x00,
		0x6a, 0xff,
		0x68, '?', '?', '?', '?',
		0x64, 0xa1, 0x00, 0x00, 0x00, 0x00
	};

	F_VMGetCallStack = (PFUNC_F_CALL)loader_search_memory(pattern);

	if (F_VMGetCallStack == nullptr)
	{
		loader_log_error("roa-debug: could not find F_VMGetCallStack()");
	}
}