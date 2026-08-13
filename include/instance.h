#pragma once

#include "yoyo.h"

using SIGNATURE_PTR_RVALUE = void* (__cdecl*)(RValue* _pV);
using SIGNATURE_CODE_FUNC_FIND = bool(__cdecl*)(const char* func_identifier, int* param_2);
using SIGNATURE_SCRIPT_PERFORM = bool(__cdecl*)(int param_1, CInstance* param_2, CInstance* param_3, int param_4, RValue* param_5, DynamicArrayOfRValue* param_6);

void prepare_global_instance();