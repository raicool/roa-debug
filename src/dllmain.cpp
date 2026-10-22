#define WIN32_MEAN_AND_LEAN
#define IMGUI_DEFINE_MATH_OPERATORS

#include <loader/info.h>
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

LOADER_MOD_ENTRY initialize(loader_info* info)
{
	loader_mod& instance = loader_mod::get();
	instance.name = "roa-debug";
	instance.description = "enables various gamemaker debug features for rivals of aether";
	instance.commit_hash = std::stoi(GIT_COMMIT_HASH);
	instance.repository = "https://github.com/raicool/roa-debug";

	pattern_find_object_hash();
	pattern_find_rng_index();
	pattern_find_VMGetCallStack();

	prepare_console_vtable_hook();
	prepare_global_instance();

	imgui_init();

	loader_log_info("welcome to roa-debug version {}", VERSION_STR);

	// add a callback to be called whenever a frame is rendered
	loader_add_present_callback(dx_present);

	// add a callback to be called whenever rival's window attempts to process an event
	loader_add_wndproc_callback(handle_wndproc);

	return instance;
}