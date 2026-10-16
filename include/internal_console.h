#pragma once

#include <vector>
#include "yoyo.h"

struct ItemEntry
{
	std::string vm_callstack;
	std::string msg;
};

struct InternalConsole
{
	std::vector<ItemEntry> items;

	void print(const char* msg);
};