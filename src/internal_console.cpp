#include "internal_console.h"

#include "instance.h"
#include "yoyo.h"

#include <loader/yyc.h>

#include <memory>

extern PFUNC_F_CALL F_VMGetCallStack;

void InternalConsole::print(const char* msg)
{
	std::string _callstack_str;

	RValue _this_callstack;

	if (F_VMGetCallStack)
		F_VMGetCallStack(&_this_callstack, nullptr, nullptr, 0, nullptr);
	else
		return;

	if (_this_callstack.value_array)
	{
		RefDynamicArrayOfRValue* arr_ptr = reinterpret_cast<RefDynamicArrayOfRValue*>(_this_callstack.value_array);

		for (int i = 0; i <= arr_ptr->pArray->length; i++)
		{
			RValue* s = &arr_ptr->pArray->arr[i];

			if (!s)
				break;

			if (s->type == GML_TYPE_STRING)
			{
				_callstack_str.append(s->value_stringref->string);
				_callstack_str.push_back('\n');
			}
			else
			{
				break;
			}
		}
	}

	items.push_back({ _callstack_str, msg });
	if (items.size() >= 500)
	{
		items.erase(items.begin());
	}
}