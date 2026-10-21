#pragma once

#include "imgui.h"

void imgui_init();
void imgui_render();

struct YYObjectBase;
struct RefDynamicArrayOfRValue;
struct CCode;
struct RValue;

void table_entry(const char* name, ImGuiDataType type, void* data, bool readonly = false, const char* format = nullptr);
void table_entry_object(const char* name, YYObjectBase* object, bool readonly = false);
void table_entry_dynarray(const char* name, RefDynamicArrayOfRValue* data, bool readonly = false);
void table_entry_code(const char* name, CCode* code, bool readonly = false);
void table_entry_rvalue(const char* name, RValue* data, bool readonly = false);