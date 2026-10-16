#include "yoyo.h"

#include "utils.h"
#include "internal_console.h"

#include <loader/memory.h>

#include <stdarg.h>
#include <iostream>
#include <vector>
#include <cstdarg>

InternalConsole g_dbgstream;
InternalConsole g_relstream;

tagIConsole* dbg_csol = nullptr;
tagIConsole* rel_csol = nullptr;

bool g_consoleprint_detour_created = false;
bool g_print_to_console = false;

void(*tagIConsole_print_orig)(tagIConsole*, const char*, ...) = nullptr;
void tagIConsole_print_detour(tagIConsole* this_console, const char* msg, ...)
{
	char buffer[512];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(buffer, 512, msg, ap);
	
	if (this_console == dbg_csol)
	{
		g_dbgstream.print(buffer);
		if (g_print_to_console)
			loader_log_debug(buffer);
	}
	else if (this_console == rel_csol)
	{
		g_relstream.print(buffer);
		if (g_print_to_console)
			loader_log_trace(buffer);
	}

	return tagIConsole_print_orig(this_console, msg, ap);
	va_end(ap);
}

void create_console_print_hook(void* addr)
{
	loader_hook_create(
		addr,
		reinterpret_cast<void*>(tagIConsole_print_detour),
		reinterpret_cast<void**>(&tagIConsole_print_orig)
	);
	loader_hook_enable(addr);

	loader_log_info("created tagIConsole::print() hook successfully");
}

void prepare_console_vtable_hook()
{
	std::vector<uint8_t> pattern_dbgstream
	{
		0x8b, 0x43, 0x54,
		0x8b, 0x73, 0x5c,
		0x89, 0x44, 0x24, 0x10,
		0x8b, 0x43, 0x58,
		0x57,
		0x8b, 0x7b, 0x50,
		0x89, 0x44, 0x24, 0x10,
		0x74, '?',
		0xa1, '?', '?', '?', '?',
		0x56
	};

	std::vector<uint8_t> pattern_relstream
	{
		0xff, 0xb5, 0xf4, 0xfb, 0xff, 0xff, // push dword ptr [ebp + 0xfffffbf4
		0x52, // push edx
		0x51, // push ecx
		0x53, // push ebx
		0xe8, '?', '?', '?', '?', // call MemoryManager::_Alloc
		0x8b, 0xf8, // mov edi, ret
		0x83, 0xc4, 0x10, // add esp, 0x10
		0x85, 0xff, // text edi, edi
		0x75, '?' // jnz LAB_0559c6e6
	};

	uint32_t ptr_dbgstream = (uint32_t)loader_search_memory(pattern_dbgstream);
	uint32_t ptr_relstream = (uint32_t)loader_search_memory(pattern_relstream);

	if (ptr_dbgstream)
	{
		dbg_csol = **(tagIConsole***)INCR_PTR(ptr_dbgstream, 24);
		if (g_consoleprint_detour_created == false)
		{
			create_console_print_hook((*dbg_csol->vtable)[2]);

			g_consoleprint_detour_created = true;
		}
		loader_log_trace("dbg_csol = 0x{:08x}", (uint32_t)dbg_csol);
	}

	if (ptr_relstream)
	{
		rel_csol = **(tagIConsole***)INCR_PTR(ptr_relstream, 33);
		if (g_consoleprint_detour_created == false)
		{
			create_console_print_hook((*rel_csol->vtable)[2]);

			g_consoleprint_detour_created = true;
		}
		loader_log_trace("rel_csol = 0x{:08x}", (uint32_t)rel_csol);
	}
}