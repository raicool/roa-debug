#include "imgui_window.h"

#include "internal_console.h"
#include "instance.h"
#include "mod.h"
#include "steam_lobby_list.h"
#include "yoyo.h"

#include <loader/d3d11_hook.h>

#include <implot.h>

#include <format>

#define TOOLTIP(x, ...) if (ImGui::IsItemHovered()) ImGui::SetTooltip(x, __VA_ARGS__)

extern ObjectsHash** g_ObjectHash;
extern int** g_RngIndex;
extern int** state;

extern InternalConsole g_dbgstream;
extern InternalConsole g_relstream;

int state_prev[16]{ 0 };

const std::string window_name = std::format("{}", MOD_NAME);

constexpr ImGuiTreeNodeFlags treenode_flags = ImGuiTreeNodeFlags_LabelSpanAllColumns | ImGuiTreeNodeFlags_DrawLinesFull;

enum RValueTypeColorEnum : uint32_t
{
	TYPE_REAL = IM_COL32(110, 180, 255, 255), // BLUE
	TYPE_STRING = IM_COL32(119, 204, 75, 255), // GREEN
	TYPE_ARRAY = IM_COL32(255, 199, 110, 255), // LIGHT ORANGE
	TYPE_POINTER = IM_COL32(110, 255, 115, 255), // LIGHT GREEN
	TYPE_UNDEFINED = IM_COL32(127, 127, 127, 255), // GRAY
	TYPE_OBJECT = IM_COL32(24, 82, 31, 255), // DARK GREEN
	TYPE_INTEGER = IM_COL32(247, 45, 45, 255), // RED
	TYPE_VECTOR = IM_COL32(165, 3, 252, 255), // PURPLE
	TYPE_BOOLEAN = IM_COL32(255, 166, 190, 255), // SALMON
	TYPE_CODE = IM_COL32(194, 78, 149, 255), // PINK
};

struct RValueDisplaySpec
{
	ImGuiDataType imgui_type = ImGuiDataType_U32;
	const char* format = nullptr;
	const char* type_string = "???";
	uint32_t type_color = RValueTypeColorEnum::TYPE_UNDEFINED;
};

const std::unordered_map<int, RValueDisplaySpec> g_rvalue_display_specs =
{
	{
		GML_TYPE_REAL,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_Double,
			.type_string = "real",
			.type_color = RValueTypeColorEnum::TYPE_REAL
		}
	},
	{
		GML_TYPE_STRING,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_String,
			.type_string = "string",
			.type_color = RValueTypeColorEnum::TYPE_STRING
		}
	},
	{
		GML_TYPE_ARRAY,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_U32,
			.format = "0x%08x",
			.type_string = "array",
			.type_color = RValueTypeColorEnum::TYPE_ARRAY
		}
	},
	{
		GML_TYPE_POINTER,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_U32,
			.format = "0x%08x",
			.type_string = "pointer",
			.type_color = RValueTypeColorEnum::TYPE_POINTER
		}
	},
	{
		GML_TYPE_UNDEFINED,
		RValueDisplaySpec
		{
			.type_color = RValueTypeColorEnum::TYPE_UNDEFINED
		}
	},
	{
		GML_TYPE_OBJECT,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_U32,
			.format = "0x%08x",
			.type_string = "object",
			.type_color = RValueTypeColorEnum::TYPE_OBJECT
		}
	},
	{
		GML_TYPE_INT32,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_S32,
			.type_string = "int32",
			.type_color = RValueTypeColorEnum::TYPE_INTEGER
		}
	},
	{
		GML_TYPE_VEC4,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_U32,
			.format = "0x%08x",
			.type_string = "vec4",
			.type_color = RValueTypeColorEnum::TYPE_VECTOR
		}
	},
	{
		GML_TYPE_VEC44,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_U32,
			.format = "0x%08x",
			.type_string = "vec44",
			.type_color = RValueTypeColorEnum::TYPE_VECTOR
		}
	},
	{
		GML_TYPE_INT64,
		RValueDisplaySpec
		{
			.imgui_type = ImGuiDataType_S64,
			.type_string = "int64",
			.type_color = RValueTypeColorEnum::TYPE_INTEGER
		}
	},
};

AsyncSteamAPICaller g_steamapi_request;
CInstance* g_current_instance;
RefDynamicArrayOfRValue* g_instance_variable_names;

enum
{
	WINDOW_GLOBALS     = (1 << 0),
	WINDOW_DBG_CONSOLE = (1 << 1),
	WINDOW_LOBBY_LIST  = (1 << 2),

	WINDOW_DEFAULTS = WINDOW_GLOBALS | WINDOW_DBG_CONSOLE | WINDOW_LOBBY_LIST
};

enum lobby_game_type
{
	UNKNOWN = 0,
	EXHIBITION = '1',
	PRIVATE = '2',
	RANKED = '3',
};

constexpr ImVec4 color_rngval_active = ImVec4(0.68, 0.92, 0.57, 0.65);
constexpr ImVec4 color_rngval_update = ImVec4(0.92, 0.57, 0.57, 0.65);

uint32_t windows_flag = WINDOW_DEFAULTS;

ImVec4 imvec4_mix(ImVec4 r, ImVec4 l)
{
	return ImVec4(
		(r.x + l.x) / 2.0,
		(r.y + l.y) / 2.0,
		(r.z + l.z) / 2.0,
		(r.w + l.w) / 2.0
	);
}

char instance_sel_name[128];
bool instance_selected;
void draw_instances(const LinkedList<CInstancePadded>* inst_list)
{
	CInstance* instance = inst_list->m_First->instance;
	for (uint32_t idx = 0; idx < inst_list->m_Count; idx++)
	{
		sprintf_s(instance_sel_name, 128, "%i (0x%08x)", instance->id, instance);
		ImGui::Selectable(instance_sel_name, &instance_selected);

		if (instance_selected)
		{
			g_current_instance = instance;

			RValue val;
			val.value_object = (YYObjectBase*)g_current_instance;
			val.type = GML_TYPE_OBJECT;
			if (g_instance_variable_names)
			{
				g_instance_variable_names->refcount = -1;
			}
			g_instance_variable_names = loader_get_instance_variable_names(val);

			instance_selected = false;
		}

		instance = instance->next;
	}
}

double last_memory_graph_update;
std::vector<float> memory_usage_x;
std::vector<float> memory_usage_y;

uint32_t highest_mem_usage = 0;
uint32_t lowest_mem_usage = 0;
void render_memoryusage()
{
	MemoryManager* _memorymanager = loader_get_memory_manager();

	if (ImGui::GetTime() - last_memory_graph_update > 0.25)
	{
		last_memory_graph_update = ImGui::GetTime();

		memory_usage_y.emplace_back(_memorymanager->total);

		if (_memorymanager->total > highest_mem_usage)
		{
			highest_mem_usage = _memorymanager->total;
		}

		if (_memorymanager->total < lowest_mem_usage || lowest_mem_usage == 0)
		{
			lowest_mem_usage = _memorymanager->total;
		}

		memory_usage_x.emplace_back(last_memory_graph_update);
	}

	if (ImGui::Begin("Memory Statistics"))
	{
		ImGui::Text("Memory Used: %u bytes", _memorymanager->in_use);
		ImGui::Text("Memory Total: %u bytes", _memorymanager->total);
		ImGui::Text("Memory Maximum: %u bytes", _memorymanager->max_size);

		const float _memory_usage_percentage = (float)_memorymanager->in_use / (float)_memorymanager->total;
		ImGui::ProgressBar(_memory_usage_percentage);

		if (ImPlot::BeginPlot("Memory Usage"))
		{
			const auto plot_color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
			ImPlotSpec line_spec;
			line_spec.FillAlpha = 0.5;
			line_spec.FillColor = ImVec4(1.0f, 0.5f, 0.0f, 0.5f);
			line_spec.LineColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
			line_spec.LineWeight = 2;
			line_spec.Flags = ImPlotItemFlags_NoLegend;

			ImPlot::SetupAxes("Time", "Memory Total (bytes)");
			ImPlot::SetupAxesLimits(0, last_memory_graph_update, lowest_mem_usage, highest_mem_usage, ImPlotCond_Always);
			ImPlot::PlotShaded("Memory Total", memory_usage_x.data(), memory_usage_y.data(), memory_usage_x.size(), -INFINITY, line_spec);
			ImPlot::EndPlot();
		}
	}
	ImGui::End();
}

bool object_treenode(const CObjectGM* object)
{
	bool pressed = ImGui::TreeNodeEx(object->m_Name, treenode_flags);

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, 0xff7f7f7f);
	ImGui::Text("(%i instances)", object->m_Instances.m_Count);
	ImGui::PopStyleColor();

	return pressed;
}

void draw_callstack_tooltip(std::string& vm_callstack)
{
	ImGui::BeginTooltip();

	ImGui::Text("Call Stack");
	ImGui::Separator();

	ImGui::PushStyleColor(ImGuiCol_Text, 0xff907f7f);
	ImGui::Text(vm_callstack.c_str());
	ImGui::PopStyleColor();

	ImGui::EndTooltip();
}

std::string get_game_type_string(CSteamID lobby_id)
{
	const char* _gametype_val = SteamMatchmaking()->GetLobbyData(lobby_id, "gametype");
	const char* _FFA_val = SteamMatchmaking()->GetLobbyData(lobby_id, "FFA");
	const char* _UGC_val = SteamMatchmaking()->GetLobbyData(lobby_id, "UGC");
	const char* _2v2_val = SteamMatchmaking()->GetLobbyData(lobby_id, "2v2");

	uint8_t gametype_u8 = _gametype_val[0];
	std::string game_type = "";
	std::vector<const char*> extra_game_types;
	bool has_extra_game_types = false;

	switch (gametype_u8)
	{
	case lobby_game_type::EXHIBITION: game_type.append("Casual"); break;
	case lobby_game_type::PRIVATE: game_type.append("Friendlies"); break;
	case lobby_game_type::RANKED: game_type.append("Ranked"); break;
	default: game_type.append("Unknown");
	}

	if (_FFA_val[0] == '1')
	{
		has_extra_game_types = true;
		extra_game_types.emplace_back("FFA");
	}

	if (_UGC_val[0] == '1')
	{
		has_extra_game_types = true;
		extra_game_types.emplace_back("Workshop");
	}

	if (_2v2_val[0] == '1')
	{
		has_extra_game_types = true;
		extra_game_types.emplace_back("2v2");
	}

	if (has_extra_game_types)
	{
		game_type.push_back('(');
		for (auto& _type : extra_game_types)
		{
			game_type.append(_type);
			game_type.push_back(',');
		}
		game_type.push_back(')');
	}

	return game_type;
}

void render_lobby_list()
{
	if (ImGui::Begin("Running Lobbies"))
	{
		if (ImGui::Button("Refresh lobby list"))
		{
			g_steamapi_request.request_lobbies();
			g_steamapi_request.request_player_count();
		}
		ImGui::SameLine();
		ImGui::Text("%i players online.", g_steamapi_request.player_count);

		ImGuiStyle& style = ImGui::GetStyle();
		const float footer_height_to_reserve = style.SeparatorSize + style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginTable("##", 4, ImGuiTableFlags_Borders, ImVec2(0, -footer_height_to_reserve)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Lobby Name", ImGuiTableColumnFlags_None);
			ImGui::TableSetupColumn("Region", ImGuiTableColumnFlags_None);
			ImGui::TableSetupColumn("Lobby Type", ImGuiTableColumnFlags_None);
			ImGui::TableSetupColumn("Player Count", ImGuiTableColumnFlags_None);
			ImGui::TableHeadersRow();
			
			for (auto& _entry : g_steamapi_request.lobbies)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				ImGui::Text(SteamMatchmaking()->GetLobbyData(_entry, "title"));
				ImGui::TableNextColumn();

				ImGui::TableNextColumn();
				
				ImGui::Text(get_game_type_string(_entry).c_str());

				ImGui::TableNextColumn();
				const uint32_t lobby_member_count = SteamMatchmaking()->GetNumLobbyMembers(_entry);
				const uint32_t lobby_member_limit = SteamMatchmaking()->GetLobbyMemberLimit(_entry);
				const std::string player_amount = std::format(
					"{}/{}",
					lobby_member_count,
					lobby_member_limit
				);

				ImGui::Text(player_amount.c_str());
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void table_entry(const char* name, ImGuiDataType type, void* data, bool readonly = false, const char* format = nullptr)
{
	constexpr char default_input_flags = ImGuiInputTextFlags_EnterReturnsTrue;

	ImGui::PushID(data);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Text(name);
	ImGui::TableSetColumnIndex(1);
	if (data)
	{
		switch (type)
		{
		case ImGuiDataType_Bool:
			ImGui::Checkbox(name, (bool*)data);
			break;

		case ImGuiDataType_String:
			ImGui::Text((char*)data);
			break;

		default:
			ImGui::InputScalar("##", type, data, nullptr, nullptr, format, default_input_flags | (readonly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None));
		}
	}
	ImGui::PopID();
}

void table_entry_object(const char* name, YYObjectBase* object, bool readonly = false)
{
	ImGui::PushID(name);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	
	if (ImGui::TreeNodeEx(name, treenode_flags))
	{
		if (object)
		{
			table_entry("m_Capacity", ImGuiDataType_U32, &object->m_Capacity, readonly);
			table_entry("m_Class", ImGuiDataType_U32, &object->m_Capacity, readonly);
			table_entry("m_CurrentSlot", ImGuiDataType_S32, &object->m_CurrentSlot, readonly);
			table_entry("m_DefineOwnProperty", ImGuiDataType_U32, &object->m_DefineOwnProperty, readonly, "0x%08x");
			table_entry("m_DeleteOwnProperty", ImGuiDataType_U32, &object->m_DeleteOwnProperty, readonly, "0x%08x");
			table_entry("m_Flags", ImGuiDataType_U32, &object->m_Flags, readonly);
			table_entry("m_GCCreationFrame", ImGuiDataType_S32, &object->m_GCCreationFrame, readonly);
			table_entry("m_GCGeneration", ImGuiDataType_S32, &object->m_GCGeneration, readonly);
			table_entry("m_GetProperty", ImGuiDataType_U32, &object->m_GetProperty, readonly, "0x%08x");
			table_entry("m_ObjectKind", ImGuiDataType_S32, &object->m_ObjectKind, readonly);
			table_entry_object("m_pNextObject", object->m_pNextObject, readonly);
			table_entry_object("m_pPrevObject", object->m_pNextObject, readonly);
			table_entry_object("m_prototype", object->m_prototype, readonly);
			table_entry("m_RValueInitType", ImGuiDataType_S32, &object->m_RValueInitType, readonly);
			table_entry("m_VariableCount", ImGuiDataType_U32, &object->m_VariableCount, readonly);
			table_entry("m_Visited", ImGuiDataType_U32, &object->m_Visited, readonly);
			table_entry("m_VisitedGC", ImGuiDataType_U32, &object->m_VisitedGC, readonly);
			table_entry("m_WeakRef", ImGuiDataType_U32, &object->m_WeakRef, readonly, "0x%08x");
			table_entry("m_WeakRefCount", ImGuiDataType_U32, &object->m_WeakRefCount, readonly);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void table_entry_code(const char* name, CCode* code, bool readonly = false)
{
	ImGui::PushID(name);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);

	auto tree_label = [&name]() -> bool
		{
			bool pressed = ImGui::TreeNodeEx(name, treenode_flags);
			ImGui::SameLine(0, 0);
			ImGui::Text(": ");
			ImGui::SameLine(0, 0);
			ImGui::PushStyleColor(ImGuiCol_Text, RValueTypeColorEnum::TYPE_CODE);
			ImGui::Text("script");
			ImGui::PopStyleColor();
			return pressed;
		};

	if (tree_label())
	{
		if (code)
		{
			table_entry_code("m_Next", code->m_Next);
			table_entry("m_Kind", ImGuiDataType_S32, &code->m_Kind, readonly);
			table_entry("m_Compiled", ImGuiDataType_S32, &code->m_Compiled, readonly);
			table_entry("m_Str", ImGuiDataType_String, (void*)code->m_Str, true);
			table_entry("m_VmInstance", ImGuiDataType_U32, &code->m_VmInstance, readonly, "0x%08x");
			table_entry("m_VmDebugInfo", ImGuiDataType_U32, &code->m_VmDebugInfo, readonly, "0x%08x");
			table_entry("m_Name", ImGuiDataType_String, (void*)code->m_Name, true);
			table_entry("m_CodeIndex", ImGuiDataType_S32, &code->m_CodeIndex, readonly);
			table_entry("m_Watch", ImGuiDataType_Bool, &code->m_Watch, readonly);
			table_entry("m_Offset", ImGuiDataType_S32, &code->m_Offset, readonly);
			table_entry("m_LocalsCount", ImGuiDataType_S32, &code->m_LocalsCount, readonly);
			table_entry("m_ArgsCount", ImGuiDataType_S32, &code->m_ArgsCount, readonly);
			table_entry("m_Flags", ImGuiDataType_S32, &code->m_Flags, readonly);
			table_entry_object("m_Prototype", code->m_Prototype, readonly);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void table_entry_rvalue(const char* name, RValue* data, bool readonly = false)
{
	RValueDisplaySpec spec;
	if (g_rvalue_display_specs.contains(data->type))
	{
		spec = g_rvalue_display_specs.at(data->type);
	}

	ImGui::PushID(name);
	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);
	ImGui::Text("%s: ", name);
	TOOLTIP("Address: 0x%08x", (void*)data);
	ImGui::SameLine(0, 0);
	ImGui::PushStyleColor(ImGuiCol_Text, spec.type_color);
	ImGui::Text(spec.type_string);
	ImGui::PopStyleColor();

	ImGui::TableSetColumnIndex(1);

	RefDynamicArrayOfRValue* array = nullptr;
	switch (data->type)
	{
	case GML_TYPE_STRING:
		ImGui::Text(data->value_stringref->string);
		break;

	case GML_TYPE_ARRAY:
		ImGui::InputScalar("##", spec.imgui_type, &data->value_s64, nullptr, nullptr, spec.format, readonly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
		ImGui::SetNextItemStorageID(reinterpret_cast<ImGuiID>(data));
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		if (ImGui::TreeNodeEx("Values", treenode_flags))
		{
			array = (RefDynamicArrayOfRValue*)data->value_array;

			if (array->pArray)
			{
				for (size_t i = 0; i < array->pArray->length; i++)
				{
					const std::string array_offset_string = std::format("{}[{}]", name, i);
					table_entry_rvalue(array_offset_string.c_str(), &array->pArray->arr[i], readonly);
				}
			}
			ImGui::TreePop();
		}
		break;

	default:
		ImGui::InputScalar("##", spec.imgui_type, &data->value_s64, nullptr, nullptr, spec.format, readonly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
	}

	ImGui::PopID();
}

void table_instance_values()
{
	if (ImGui::TreeNodeEx("Instance Variables", treenode_flags))
	{
		RValue _rv_instance;
		_rv_instance.value_instance = g_current_instance;
		_rv_instance.type = GML_TYPE_OBJECT;

		if (g_instance_variable_names->pArray)
		{
			for (size_t i = 0; i < g_instance_variable_names->pArray->length; i++)
			{
				GMLStringRef* string_ref = g_instance_variable_names->pArray->arr[i].value_stringref;

				RValue* variable = loader_get_instance_variable(_rv_instance, string_ref->string);

				if (variable)
				{
					table_entry_rvalue(string_ref->string, variable);
				}
			}
		}
		ImGui::TreePop();
	}
}

void render_instance_details()
{
	constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_HighlightHoveredColumn;

	if (ImGui::Begin("Instance Details"))
	{
		if (ImGui::BeginTable("##", 2, flags))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Variable", ImGuiTableColumnFlags_None);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None);
			ImGui::TableHeadersRow();

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			if (ImGui::TreeNodeEx("Built-in Variables", treenode_flags))
			{
				table_entry("create_counter", ImGuiDataType_U64, &g_current_instance->create_counter);
				table_entry("id", ImGuiDataType_U32, &g_current_instance->id);
				table_entry("object_index", ImGuiDataType_U32, &g_current_instance->object_index);
				table_entry("sprite_id", ImGuiDataType_U32, &g_current_instance->sprite_id);
				table_entry("image_index", ImGuiDataType_Float, &g_current_instance->image_index);
				table_entry("image_speed", ImGuiDataType_Float, &g_current_instance->image_speed);
				table_entry("image_scale_x", ImGuiDataType_Float, &g_current_instance->image_scale_x);
				table_entry("image_scale_y", ImGuiDataType_Float, &g_current_instance->image_scale_y);
				table_entry("image_angle", ImGuiDataType_Float, &g_current_instance->image_angle);
				table_entry("image_alpha", ImGuiDataType_Float, &g_current_instance->image_alpha);
				table_entry("image_blend", ImGuiDataType_U32, &g_current_instance->image_blend);
				table_entry("x", ImGuiDataType_Float, &g_current_instance->x);
				table_entry("y", ImGuiDataType_Float, &g_current_instance->y);
				table_entry("x_start", ImGuiDataType_Float, &g_current_instance->x_start);
				table_entry("y_start", ImGuiDataType_Float, &g_current_instance->y_start);
				table_entry("x_previous", ImGuiDataType_Float, &g_current_instance->x_previous);
				table_entry("y_previous", ImGuiDataType_Float, &g_current_instance->y_previous);
				table_entry("direction", ImGuiDataType_Float, &g_current_instance->direction);
				table_entry("speed", ImGuiDataType_Float, &g_current_instance->speed);
				table_entry("friction", ImGuiDataType_Float, &g_current_instance->friction);
				table_entry("gravity_dir", ImGuiDataType_Float, &g_current_instance->gravity_dir);
				table_entry("gravity", ImGuiDataType_Float, &g_current_instance->gravity);
				table_entry("hspeed", ImGuiDataType_Float, &g_current_instance->hspeed);
				table_entry("vspeed", ImGuiDataType_Float, &g_current_instance->vspeed);

				ImGui::TreePop();
			}

			table_entry_code("init_code", g_current_instance->init_code);
			table_entry_code("precreate_code", g_current_instance->precreate_code);

			table_entry_object("YYObjectBase", g_current_instance);
		
			if (g_instance_variable_names)
			{
				ImGui::PushID(g_current_instance->id);
				table_instance_values();
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

// some notes:
// RNG:
//	current rng value = state[g_RngIndex] both variables are set by function YYRandom()
//	for the most part rng is only advanced by the function ParticleSystem_Emitter_Burst()
//	not a lot of other things in the game will advance rng
//	when a new room is loaded into the rng is reset back to a default value (usually 0x00000029)
//	the first 2 bytes will sometimes be reset to 7fff instead of 0000, but it depends on where g_RngIndex ends up after the reset
// 
//	g_ObjectHash is a Hashmap that contains pointers to all currently loaded objects in the current room
void render_globals()
{
	ObjectsHash* object_hash = *g_ObjectHash;
	if (ImGui::Begin("Active Instances"))
	{
		for (int i = 0; i < object_hash->m_count; i++)
		{
			const CObjectGM* this_object = object_hash->find_object(i);

			if (!this_object) continue;

			if (this_object->m_Instances.m_Count > 0)
			{
				if (object_treenode(this_object))
				{
					draw_instances(&this_object->m_Instances);

					ImGui::TreePop();
				}
			}
		}
	}
	ImGui::End();

	if (g_current_instance)
	{
		bool found = false;
		object_hash->for_each([&found](CObjectGM* object)
			{
				if (object->m_Instances.m_Count <= 0)
				{
					return;
				}

				CInstance* inst = object->m_Instances.m_First->instance;
				for (uint32_t idx = 0; idx < object->m_Instances.m_Count; idx++)
				{
					if (inst == g_current_instance)
					{
						found = true;
					}

					inst = inst->next;
				}
			});

		if (found)
		{
			render_instance_details();
		}
		else
		{
			g_current_instance = nullptr;
		}
	}

	if (ImGui::Begin(window_name.c_str()))
	{
		const int _v_RngIndex = **g_RngIndex;
		int* _p_state = *state;

		std::string _globals_str = std::format(
			"YYC Global Addresses: \n"
			"\tg_ObjectHash = 0x{:08x}\n"
			"\tg_RngIndex = (@0x{:08x})\n"
			"\n"
			"YYC Globals:\n"
			"\tg_RngIndex = {:>2}/16\n"
			"\tstate[g_RngIndex] = 0x{:08x}\n",
			(uint32_t)g_ObjectHash,
			(uint32_t)g_RngIndex,
			(unsigned)_v_RngIndex,
			(unsigned)_p_state[_v_RngIndex]
		);

		ImGui::Text(_globals_str.c_str());

		if (ImGui::TreeNode("Rng State Values"))
		{
			if (ImGui::BeginTable("##", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("index", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_None);
				ImGui::TableHeadersRow();

				std::string _state_offset_str;
				auto back_color = ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg);
				auto back_color_alt = ImGui::GetStyleColorVec4(ImGuiCol_TableRowBgAlt);

				auto back_color_active = imvec4_mix(back_color, color_rngval_active);
				auto back_color_alt_active = imvec4_mix(back_color_alt, color_rngval_active);

				auto back_color_update = imvec4_mix(back_color, color_rngval_update);
				auto back_color_update_active = imvec4_mix(back_color_alt, color_rngval_update);

				for (uint8_t i = 0; i <= 0xf; i++)
				{
					if (state_prev[i] ^ _p_state[i])
					{
						// highlight updated value with red
						ImGui::PushStyleColor(ImGuiCol_TableRowBg, back_color_update);
						ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, back_color_update_active);
					}
					else if (i == _v_RngIndex)
					{
						// highlight current value with green
						ImGui::PushStyleColor(ImGuiCol_TableRowBg, back_color_active);
						ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, back_color_alt_active);
					}
					else
					{
						ImGui::PushStyleColor(ImGuiCol_TableRowBg, back_color);
						ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, back_color_alt);
					}
					state_prev[i] = _p_state[i];
					
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);

					_state_offset_str = std::format("state[{}]", i);
					ImGui::Text(_state_offset_str.c_str());

					ImGui::TableSetColumnIndex(1);

					ImGui::PushID(_v_RngIndex);
					ImGui::InputScalar(
						"##",
						ImGuiDataType_U32,
						&_p_state[i],
						nullptr,
						nullptr,
						"%08x",
						ImGuiInputTextFlags_CharsHexadecimal
					);
					ImGui::PopID();

					ImGui::PopStyleColor(2);
				}

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

char inp_buffer[256];
bool scroll_to_bottom = false;
bool auto_scroll = false;
void render_dbg_console()
{
	if (ImGui::Begin("Debug Console"))
	{
		ImGui::Checkbox("Auto Scroll", &auto_scroll);
		if (ImGui::Button("Scroll to Bottom"))
			scroll_to_bottom = true;

		InternalConsole* current_console = nullptr;
		if (ImGui::BeginTabBar("##Console"))
		{
			if (ImGui::BeginTabItem("Release Console"))
			{
				current_console = &g_relstream;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Debug Console"))
			{
				current_console = &g_dbgstream;
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
			
		if (current_console)
		{
			ImGuiStyle& style = ImGui::GetStyle();
			const float footer_height_to_reserve = style.SeparatorSize + style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
			if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
			{
				for (auto& _entry : current_console->items)
				{
					ImGui::Text(_entry.msg.c_str());

					if (ImGui::IsItemHovered()) 
						draw_callstack_tooltip(_entry.vm_callstack);
				}

				if (scroll_to_bottom || auto_scroll)
				{
					ImGui::SetScrollHereY(1.0f);
					scroll_to_bottom = false;
				}
			}
			ImGui::EndChild();
			ImGui::Separator();

			ImGui::InputText("Input", inp_buffer, 256);
		}
	}
	ImGui::End();
}

void imgui_render()
{
	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	render_memoryusage();
	render_globals();
	render_dbg_console();
	render_lobby_list();
}