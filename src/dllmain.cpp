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
#include "instance.h"
#include "pattern.h"

DWORD WINAPI entry(LPVOID hModule)
{
	pattern_find_object_hash();
	pattern_find_rng_index();
	pattern_find_VMGetCallStack();

	prepare_console_vtable_hook();
	prepare_global_instance();

	imgui_init();

	// add a callback to be called whenever a frame is rendered
	loader_add_present_callback(dx_present);

	// add a callback to be called whenever rival's window attempts to process an event
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