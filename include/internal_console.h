#pragma once

#include <vector>
#include "yoyo.h"

struct item_entry
{
	std::string vm_callstack;
	std::string msg;
};

struct internal_console
{
	std::vector<item_entry> items;

	void print(const char* msg);
};