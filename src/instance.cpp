#include "instance.h"

#include "utils.h"
#include "yoyo.h"

#include <loader/memory.h>

#include <vector>

SIGNATURE_PTR_RVALUE PTR_RValue = nullptr;
SIGNATURE_CODE_FUNC_FIND Code_Function_Find = nullptr;
SIGNATURE_SCRIPT_PERFORM Script_Perform = nullptr;
int __vars_get_names__;
int __global_getter__ = 0;

CInstance* g_GlobalInstance = nullptr;
YYObjectBase* g_pGlobal = nullptr;

void prepare_Code_Function_Find()
{
	std::vector<uint8_t> pattern
	{
		0x53, 
		0x8b, 0x5c, 0x24, 0x08,
		0x55, 0x8b, 0x6c, 0x24, 0x10, 
		0xc7, 0x45, 0x00, 0xff, 0xff, 0xff, 0xff, 
		0x85, 0xdb
	};

	void* ptr_Code_Function_Find = loader_search_memory(pattern);

	if (ptr_Code_Function_Find)
	{
		Code_Function_Find = (SIGNATURE_CODE_FUNC_FIND)ptr_Code_Function_Find;
	}
}

void prepare_Script_Perform()
{
	std::vector<uint8_t> pattern
	{
		0x55, 0x8d, 0xac, 0x24, 0xfc, 0xfb, 0xff, 0xff, 0x81, 0xec, 0x04, 0x04, 0x00, 0x00, 0x6a, 0xff 
	};

	void* ptr_Script_Perform = loader_search_memory(pattern);

	if (ptr_Script_Perform)
	{
		Script_Perform = (SIGNATURE_SCRIPT_PERFORM)ptr_Script_Perform;
	}
}

void prepare_PTR_RValue()
{
	std::vector<uint8_t> pattern
	{
		0x8b, 0x54, 0x24, 0x04,
		0x56,
		0x33, 0xf6,
		0x8b, 0x4a, 0x0c,
		0x81, 0xe1, 0xff, 0xff, 0xff, 0x00
	};

	void* ptr_PTR_RValue = loader_search_memory(pattern);

	if (ptr_PTR_RValue)
	{
		PTR_RValue = (SIGNATURE_PTR_RVALUE)ptr_PTR_RValue;
	}
}

// CInstance* get_global_instance()
// {
// 	RValue result;
// 	Script_Perform(__global_getter__, nullptr, nullptr, 0, &result, nullptr);
// 	if (result.valuePointer)
// 	{
// 		g_GlobalInstance = (CInstance*)PTR_RValue(&result);
// 		Script_Perform(__self_getter__, g_GlobalInstance, g_GlobalInstance, 0, &result, nullptr);
// 		loader_log_info("Global Instance = {}", (uint32_t)g_GlobalInstance);
// 		return g_GlobalInstance;
// 	}
// 	else
// 	{
// 		loader_log_error("get_global_instance(): faled to get global instance object");
// 		return nullptr;
// 	}
// }