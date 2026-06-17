#define WIN32_MEAN_AND_LEAN
#define IMGUI_DEFINE_MATH_OPERATORS

#include <loader/yyc.h>
#include <loader/memory.h>
#include <loader/hook.h>
#include <loader/d3d11_hook.h>

#include "yoyo.h"
#include "callback.h"
#include "utils.h"
#include "console_vtable_hook.h"

PFUNC_F_CALL F_VMGetCallStack = nullptr;
PFUNC_F_CALL F_ArrayGet = nullptr;
PFUNC_F_CALL F_ArrayLength1D = nullptr;

Hash<CObjectGM>* g_ObjectHash = nullptr;
int** g_RngIndex = nullptr;
int** state = nullptr;

void find_object_hash()
{
	std::vector<uint8_t> pattern_Object_Find =
	{
		0x8b, 0x2d, '?', '?', '?', '?', // mov ebp, dword ptr [g_ObjectHash]
		0x33, 0xc9, // xor ecx, ecx
		0x56, // push esi
		0x57, // push edi
		0x8b, 0x5d, 0x00, // mov ebx, dword ptr [ebp]
		0x8b, 0xf3, // mov esi, ebx
		0x39, 0x0e, // cmp dword ptr [esi], ecx
		0x75, 0x10, // jnz 0x10
		0x8b, 0x45, 0x04 // mov eax, dword ptr [ebp + 0x4]
	};

	uint32_t ptr_Object_Find = (uint32_t)loader_search_memory(pattern_Object_Find);

	if (ptr_Object_Find)
	{
		g_ObjectHash = (Hash<CObjectGM>*)INCR_PTR(ptr_Object_Find, 2);
		loader_log_trace("g_ObjectHash = 0x{:08x}", (uint32_t)g_ObjectHash);
	}
	else
	{
		loader_log_error("roa-debug: could not find Object_Find()");
	}
}

void find_rng_index()
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
		g_RngIndex = (int**)INCR_PTR(ptr_YYRandom, 4);
		state = (int**)INCR_PTR(ptr_YYRandom, 12);
		loader_log_trace("g_RngIndex = 0x{:08x}", (uint32_t)g_RngIndex);
		loader_log_trace("state = 0x{:08x}", (uint32_t)state);
	}
	else
	{
		loader_log_error("roa-debug: could not find YYRandom()");
	}
}

void* find_VMGetCallStack()
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

	return loader_search_memory(pattern);
}

void* find_ArrayGet()
{
	std::vector<uint8_t> pattern
	{
		0x56,
		0x8b, 0x74, 0x24, 0x18,
		0x8b, 0x46, 0x0c,
		0x83, 0xf8, 0x02
	};


	return loader_search_memory(pattern);
}

void* find_ArrayLength1D()
{
	std::vector<uint8_t> pattern
	{
		0x8b, 0x44, 0x24, 0x04,
		0x8b, 0x4c, 0x24, 0x14,
		0xc7, 0x40, 0x0c, 0x00, 0x00, 0x00, 0x00
	};

	return loader_search_memory(pattern);
}

void* find_ArrayLength2D()
{
	std::vector<uint8_t> pattern
	{
		0x56,
		0x8b, 0x74, 0x24, 0x18,
		0x6a, 0x01,
		0x56,
		0xe8, '?', '?', '?', '?',
		0x8b, 0x4c, 0x24, 0x10,
		0x0f, 0x57, 0xc0,
		0x83, 0xc4, 0x08,
		0x8b, 0xd0,
		0xc7, 0x41, 0x0c, 0x00, 0x00, 0x00,
		0x00, 0xf2, 0x0f, 0x11, 0x01
	};

	return loader_search_memory(pattern);
}

DWORD WINAPI entry(LPVOID hModule)
{
	find_object_hash();
	find_rng_index();
	prepare_console_vtable_hook();
	F_VMGetCallStack = (PFUNC_F_CALL)find_VMGetCallStack();
	F_ArrayGet = (PFUNC_F_CALL)find_ArrayGet();
	F_ArrayLength1D = (PFUNC_F_CALL)find_ArrayLength1D();

	imgui_init();

	loader_add_present_callback(dx_present);

	loader_add_wndproc_callback(handle_wndproc);

	return TRUE;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);

		HANDLE hHandle = CreateThread(NULL, 0, entry, hModule, 0, NULL);
		if (hHandle != NULL)
		{
			CloseHandle(hHandle);
		}
	}
	else if (fdwReason == DLL_PROCESS_DETACH && !lpReserved)
	{
		FreeLibraryAndExitThread(hModule, 0);
	}

	return TRUE;
}