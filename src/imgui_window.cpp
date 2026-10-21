#include "imgui_window.h"

#include "internal_console.h"
#include "instance.h"
#include "mod.h"
#include "steam_lobby_list.h"
#include "utils.h"
#include "yoyo.h"

#include <loader/d3d11_hook.h>

#include <implot.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>

#include <format>

#define TOOLTIP(x, ...) if (ImGui::IsItemHovered()) ImGui::SetTooltip(x, __VA_ARGS__)

extern ObjectsHash** g_ObjectHash;
extern int** g_RngIndex;
extern int** state;
extern InternalConsole g_dbgstream;
extern InternalConsole g_relstream;

constexpr ImGuiTreeNodeFlags treenode_flags = ImGuiTreeNodeFlags_LabelSpanAllColumns | ImGuiTreeNodeFlags_DrawLinesFull;
constexpr ImPlotFlags memory_graph_plot_flags = ImPlotFlags_NoFrame | ImPlotFlags_NoTitle | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect;
constexpr ImPlotAxisFlags memory_graph_plot_axis_flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_PanStretch;

const std::string window_name = MOD_NAME;

bool window_console_open = true;
bool window_memory_stats_open = true;
bool window_main_open = false;
bool window_lobbies_open = false;
bool window_instance_open = true;
bool window_instance_browser_open = true;

float window_background_alpha = 0.78;

size_t g_instance_count = 0;
AsyncSteamAPICaller g_steamapi_request;
CInstance* g_current_instance;
RefDynamicArrayOfRValue* g_instance_variable_names;

char g_variable_filter_input_buffer[128]{ 0 };

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
			.format = "%.3g",
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

enum LobbyTypeEnum
{
	UNKNOWN = 0,
	EXHIBITION = '1',
	PRIVATE = '2',
	RANKED = '3',
};

inline ImVec4 imvec4_mix(ImVec4 r, ImVec4 l, float factor = 0.5f)
{
	factor = std::clamp(factor, 0.0f, 1.0f);

	return ImVec4(
		((r.x * (1 - factor)) + (l.x * factor)),
		((r.y * (1 - factor)) + (l.y * factor)),
		((r.z * (1 - factor)) + (l.z * factor)),
		((r.w * (1 - factor)) + (l.w * factor))
	);
}

void render_instances(const LinkedList<CInstancePadded>* inst_list)
{
	CInstance* instance = inst_list->m_First->instance;

	for (uint32_t idx = 0; idx < inst_list->m_Count; idx++)
	{
		std::string instance_name = std::format("#{} (0x{:08x})", instance->id, (uintptr_t)instance);

		ImGui::PushStyleColor(ImGuiCol_Text, RValueTypeColorEnum::TYPE_UNDEFINED);
		ImGui::Selectable(instance_name.data(), g_current_instance == instance);
		ImGui::PopStyleColor();

		if (ImGui::IsItemActivated())
		{
			g_current_instance = instance;

			if (g_instance_variable_names)
			{
				// call MemoryManager::Free on old array before loading new variable names
				if (g_instance_variable_names->pArray)
					loader_memory_free(g_instance_variable_names->pArray->arr);
			}

			// create array of MemoryManager allocated variable name strings for new instance
			g_instance_variable_names = loader_get_instance_variable_names(g_current_instance);
		}

		instance = instance->next;
	}
}

std::vector<double> plot_duration;
std::vector<double> plot_memory_usage;
std::vector<double> plot_instance_count;
double plot_last_update;
double plot_memory_graph_higher_bound = 0;
double plot_memory_graph_lower_bound = 0;
double plot_update_interval = 0.5;
double plot_depth = 10;

void render_memoryusage()
{
	MemoryManager* memory_manager = loader_get_memory_manager();

	const double current_memory_total_mb = (double)memory_manager->total / 1000000.0f;

	if (ImGui::GetTime() - plot_last_update > plot_update_interval)
	{
		plot_last_update = ImGui::GetTime();

		plot_memory_usage.emplace_back(current_memory_total_mb);

		// set memory plot bounds
		if (current_memory_total_mb > plot_memory_graph_higher_bound)
		{
			plot_memory_graph_higher_bound = current_memory_total_mb + 10;
		}

		if (current_memory_total_mb < plot_memory_graph_lower_bound || plot_memory_graph_lower_bound == 0)
		{
			plot_memory_graph_lower_bound = current_memory_total_mb - 10;
		}

		plot_instance_count.emplace_back(g_instance_count);
		plot_duration.emplace_back(plot_last_update);
	}

	if (window_memory_stats_open == false)
		return;

	if (ImGui::Begin("Memory Statistics", &window_memory_stats_open))
	{
		ImGui::Text("YYC Memory Total: %.02f MB [?]", current_memory_total_mb);
		TOOLTIP(
			"YYC Memory Total is the total amount of memory created via Gamemaker variables, rooms, resources etc. NOT the process memory total\n"
			"This is useful for tracking things like potential memory leaks within Workshop characters/stages/buddies etc"
		);

		if (ImGui::Button("Reset Memory Graph"))
		{
			plot_duration.clear();
			plot_memory_usage.clear();
			plot_instance_count.clear();
			plot_memory_graph_lower_bound = 0;
			plot_memory_graph_higher_bound = 0;
		}

		const double plot_depth_slider_min = 1;
		const double plot_depth_slider_max = 100;
		ImGui::SliderScalar("Plot Depth (seconds)", ImGuiDataType_Double, &plot_depth, &plot_depth_slider_min, &plot_depth_slider_max);
		ImGui::InputDouble("Plot Update Interval", &plot_update_interval);
		plot_update_interval = std::clamp(plot_update_interval, 0.1, (double)INT_MAX);

		if (ImPlot::BeginPlot("Memory Usage", ImVec2(-1, 0), memory_graph_plot_flags))
		{
			// MEMORY

			ImPlotSpec memory_line_spec;
			memory_line_spec.FillAlpha = 0.5;
			memory_line_spec.FillColor = ImVec4(1.0f, 0.5f, 0.0f, 0.5f);
			memory_line_spec.LineColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
			memory_line_spec.LineWeight = 1;

			std::string memory_total_text = std::vformat("Mem {:.2f} MB", std::make_format_args(current_memory_total_mb));

			ImPlot::SetupAxis(ImAxis_X1, "##", memory_graph_plot_axis_flags);
			ImPlot::SetupAxis(ImAxis_Y1, "##", memory_graph_plot_axis_flags | ImPlotAxisFlags_NoTickLabels);
			ImPlot::SetupAxisLimits(ImAxis_X1, plot_last_update - plot_depth, plot_last_update, ImPlotCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, plot_memory_graph_lower_bound, plot_memory_graph_higher_bound, ImPlotCond_Always);

			// INSTANCES

			ImPlotSpec instances_line_spec;
			instances_line_spec.LineColor = ImVec4(0.0f, 1.0, 0.5f, 1.0f);
			instances_line_spec.LineWeight = 1;

			std::string instance_count_text = std::vformat("Instance Count {}", std::make_format_args(g_instance_count));
			ImPlot::SetupAxis(ImAxis_X2, "##", memory_graph_plot_axis_flags | ImPlotAxisFlags_NoTickLabels);
			ImPlot::SetupAxis(ImAxis_Y2, "##", memory_graph_plot_axis_flags | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_RangeFit);
			//ImPlot::SetupAxisLimits(ImAxis_Y2, 0, memory_graph_instances_higher_bound, ImPlotCond_Always);

			// MEMORY
			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
			ImPlot::PlotShaded("##", plot_duration.data(), plot_memory_usage.data(), plot_duration.size(), -INFINITY, memory_line_spec);
			ImPlot::PlotLine(memory_total_text.data(), plot_duration.data(), plot_memory_usage.data(), plot_duration.size(), memory_line_spec);

			// INSTANCES
			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
			ImPlot::PlotLine(instance_count_text.data(), plot_duration.data(), plot_instance_count.data(), plot_instance_count.size(), instances_line_spec);

			ImPlot::EndPlot();
		}
	}
	ImGui::End();
}

void render_cobjectgm_treenode(const CObjectGM* object)
{
	const char* name = object->m_Name;
	auto tree_label = [&name, &object]() -> bool
		{
			bool pressed = ImGui::TreeNodeEx(name, treenode_flags);
			if (object->m_ParentObject)
			{
				TOOLTIP("Parent Object: %s", object->m_ParentObject->m_Name);
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, 0xff7f7f7f);
			ImGui::Text("(%i instances)", object->m_Instances.m_Count);
			ImGui::PopStyleColor();
			return pressed;
		};

	if (tree_label())
	{
		render_instances(&object->m_Instances);

		ImGui::TreePop();
	}
}

void render_callstack_tooltip(std::string& vm_callstack)
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
	case LobbyTypeEnum::EXHIBITION: game_type.append("Casual"); break;
	case LobbyTypeEnum::PRIVATE: game_type.append("Friendlies"); break;
	case LobbyTypeEnum::RANKED: game_type.append("Ranked"); break;
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
	if (window_lobbies_open == false)
		return;

	if (ImGui::Begin("Running Lobbies", &window_lobbies_open))
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

void table_entry(const char* name, ImGuiDataType type, void* data, bool readonly, const char* format)
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

void table_entry_object(const char* name, YYObjectBase* object, bool readonly)
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
			ImGui::PushStyleColor(ImGuiCol_Text, RValueTypeColorEnum::TYPE_OBJECT);
			ImGui::Text("object");
			ImGui::PopStyleColor();
			return pressed;
		};
	
	if (tree_label())
	{
		if (object)
		{
			table_entry("m_Capacity", ImGuiDataType_U32, &object->m_Capacity, readonly);
			table_entry("m_Class", ImGuiDataType_String, &object->m_Class, true);
			table_entry("m_CurrentSlot", ImGuiDataType_S32, &object->m_CurrentSlot, readonly);
			table_entry("m_DefineOwnProperty", ImGuiDataType_U32, &object->m_DefineOwnProperty, readonly, "0x%08x");
			table_entry("m_DeleteOwnProperty", ImGuiDataType_U32, &object->m_DeleteOwnProperty, readonly, "0x%08x");
			table_entry("m_Flags", ImGuiDataType_U32, &object->m_Flags, readonly);
			table_entry("m_GCCreationFrame", ImGuiDataType_S32, &object->m_GCCreationFrame, readonly);
			table_entry("m_GCGeneration", ImGuiDataType_S32, &object->m_GCGeneration, readonly);
			table_entry("m_GetProperty", ImGuiDataType_U32, &object->m_GetProperty, readonly, "0x%08x");
			table_entry("m_ObjectKind", ImGuiDataType_S32, &object->m_ObjectKind, readonly);

			// unsafe if no object link is present
			//table_entry_object("m_pNextObject", object->m_pNextObject, true);
			//table_entry_object("m_pPrevObject", object->m_pNextObject, true);
			//table_entry_object("m_prototype", object->m_prototype, true);

			table_entry("m_RValueInitType", ImGuiDataType_S32, &object->m_RValueInitType, readonly);
			table_entry("m_VariableCount", ImGuiDataType_U32, &object->m_VariableCount, readonly);
			table_entry("m_Visited", ImGuiDataType_U32, &object->m_Visited, readonly);
			table_entry("m_VisitedGC", ImGuiDataType_U32, &object->m_VisitedGC, readonly);
			table_entry("m_WeakRef", ImGuiDataType_U32, &object->m_WeakRef, readonly, "0x%08x");
			table_entry("m_WeakRefCount", ImGuiDataType_U32, &object->m_WeakRefCount, readonly);
		}
		ImGui::TreePop();
	}

	ImGui::TableSetColumnIndex(1);
	ImGui::InputScalar("##", ImGuiDataType_U32, &object, nullptr, nullptr, "0x%08x", ImGuiInputTextFlags_ReadOnly);
	ImGui::PopID();
}

void table_entry_code(const char* name, CCode* code, bool readonly)
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
			table_entry("_vptr$CCode", ImGuiDataType_S32, (void*)code->_vptr$CCode, true, "0x%08x");
			TOOLTIP("Address to the script's compiled code");
			table_entry("m_Kind", ImGuiDataType_S32, &code->m_Kind, readonly);
			table_entry("m_Compiled", ImGuiDataType_S32, &code->m_Compiled, readonly);
			table_entry("m_Str", ImGuiDataType_String, (void*)code->m_Str, true);
			table_entry("m_VmInstance", ImGuiDataType_U32, &code->m_VmInstance, true, "0x%08x");
			table_entry("m_VmDebugInfo", ImGuiDataType_U32, &code->m_VmDebugInfo, true, "0x%08x");
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

void table_entry_array(const char* name, ImGuiDataType type, void* data, size_t stride, size_t length, bool readonly)
{
	// NAME AND POINTER
	ImGui::PushID(name);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Text(name);
	ImGui::TableSetColumnIndex(1);
	ImGui::InputScalar("##", ImGuiDataType_U32, &data, nullptr, nullptr, "0x%08x", ImGuiInputTextFlags_ReadOnly);

	// VALUES
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::SetNextItemStorageID(reinterpret_cast<ImGuiID>(data));
	if (ImGui::TreeNodeEx("Values", treenode_flags))
	{
		char* s = reinterpret_cast<char*>(data);
		for (size_t i = 0; i < length; i++)
		{
			const std::string array_offset_string = std::format("{}[{}]", name, i);

			table_entry(array_offset_string.data(), type, s, readonly, nullptr);

			s += stride;
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

void table_entry_rvalue(const char* name, RValue* data, bool readonly)
{
	if (strlen(g_variable_filter_input_buffer) > 0)
	{
		const char* result = strstr(name, g_variable_filter_input_buffer);
		if (result == nullptr || result == name)
		{
			return;
		}
	}

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
	switch (data->type)
	{
	case GML_TYPE_STRING: // string
		if (data->value_stringref)
		{
			ImGui::Text(data->value_stringref->string);
		}
		break;

	case GML_TYPE_ARRAY: // RefDynamicArrayOfRValue
		// POINTER
		ImGui::InputScalar("##", ImGuiDataType_U32, &data->value_array, nullptr, nullptr, "0x%08x", ImGuiInputTextFlags_ReadOnly);

		// VALUES
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::SetNextItemStorageID(reinterpret_cast<ImGuiID>(data->value_array));
		if (ImGui::TreeNodeEx("Values", treenode_flags))
		{
			const auto* dynarray = static_cast<RefDynamicArrayOfRValue*>(data->value_array)->pArray;

			if (dynarray)
			{
				for (size_t i = 0; i < dynarray->length; i++)
				{
					const std::string array_offset_string = std::format("{}[{}]", name, i);
					table_entry_rvalue(array_offset_string.data(), &dynarray->arr[i], readonly);
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
		RValue instance_rval;
		instance_rval.value_object = static_cast<YYObjectBase*>(g_current_instance);
		instance_rval.type = GML_TYPE_OBJECT;

		const auto* dyn_array = g_instance_variable_names->pArray;

		if (dyn_array)
		{
			for (size_t i = 0; i < dyn_array->length; i++)
			{
				const GMLStringRef* string_ref = dyn_array->arr[i].value_stringref;

				RValue* variable = loader_get_instance_variable(instance_rval, string_ref->string);

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

	// instance browser window sets g_current_instance to nullptr if the current instance is not found/deleted
	// check if its being ran to make sure we arent dereferencing an invalid g_current_instance pointer
	if (window_instance_open == false || window_instance_browser_open == false || g_current_instance == nullptr)
		return;

	if (ImGui::Begin("Instance Details", &window_instance_open))
	{
		ImGui::Text("Current Instance: %s (#%u)", g_current_instance->object->m_Name, g_current_instance->id);

		ImGuiStyle& style = ImGui::GetStyle();
		const float footer_height_to_reserve = style.SeparatorSize + style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

		if (ImGui::BeginTable("##", 2, flags, ImVec2(0, -footer_height_to_reserve)))
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
				// missing tagYYRect bbox
				table_entry_array("timers", ImGuiDataType_S32, &g_current_instance->timers, sizeof(int), 12);
				// missing int* timeline_path
				table_entry("old_object", ImGuiDataType_U32, &g_current_instance->old_object, true, "0x%08x");
				table_entry("layer_id", ImGuiDataType_S32, &g_current_instance->layer_id);
				table_entry("mask_index", ImGuiDataType_S32, &g_current_instance->mask_index);
				table_entry("mouse_over_count", ImGuiDataType_S32, &g_current_instance->mouse_over_count);
				// missing CInstance* next
				// missing CInstance* prev
				// missing SLink collision_link
				// missing SLink dirty_link
				// missing SLink with_link
				table_entry("depth", ImGuiDataType_Float, &g_current_instance->depth);
				table_entry("current_depth", ImGuiDataType_Float, &g_current_instance->current_depth);
				table_entry("last_image_number", ImGuiDataType_Float, &g_current_instance->last_image_number);
				table_entry("collision_test_number", ImGuiDataType_U32, &g_current_instance->collision_test_number);

				ImGui::TreePop();
			}

			table_entry_code("init_code", g_current_instance->init_code);
			table_entry_code("precreate_code", g_current_instance->precreate_code);

			table_entry_object("YYObjectBase", g_current_instance);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
		
			if (g_instance_variable_names)
			{
				ImGui::PushID(g_current_instance->id);
				table_instance_values();
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		ImGui::Separator();
		if (ImGui::Button("x"))
		{
			memset(g_variable_filter_input_buffer, 0, 128);
		}
		ImGui::SameLine();
		ImGui::InputText("Filter", g_variable_filter_input_buffer, 128);
	}
	ImGui::End();
}

void render_instance_browser()
{
	if (window_instance_browser_open == false)
		return;

	g_instance_count = 0;
	bool found = false;

	ObjectsHash* object_hash = *g_ObjectHash;

	if (ImGui::Begin("Active Instances", &window_instance_browser_open))
	{
		object_hash->for_each([&found](const CObjectGM* this_object)
			{
				if (this_object->m_Instances.m_Count <= 0)
				{
					return;
				}

				g_instance_count += this_object->m_Instances.m_Count;

				render_cobjectgm_treenode(this_object);

				CInstance* inst = this_object->m_Instances.m_First->instance;
				for (uint32_t idx = 0; idx < this_object->m_Instances.m_Count; idx++)
				{
					if (inst == g_current_instance)
					{
						found = true;
					}

					inst = inst->next;
				}
			});
	}
	ImGui::End();

	if (g_current_instance && found == false)
	{
		// current instance was deleted? whatever the case the instance pointer was not found in objects hash
		g_current_instance = nullptr;
	}
}

int state_prev[16]{ 0 };
float state_background_glow_timer[16]{ 0 };

constexpr ImVec4 color_rngval_update = ImVec4(0.68, 1.0f, 0.57, 0.75f);
constexpr ImVec4 color_rngval_active = ImVec4(0.92, 0.57, 0.57, 1.0f);

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
	if (window_main_open == false)
		return;

	if (ImGui::Begin(window_name.data(), &window_main_open))
	{
		const int _v_RngIndex = **g_RngIndex;
		int* _p_state = *state;

		std::string _globals_str = std::format(
			"YYC Global Addresses: \n"
			"\tg_ObjectHash = 0x{:08x}\n"
			"\tg_RngIndex = (@0x{:08x})\n"
			"\n"
			"YYC Globals:\n"
			"\tg_RngIndex = {:>2}/15\n"
			"\tstate[g_RngIndex] = 0x{:08x}\n",
			(uint32_t)g_ObjectHash,
			(uint32_t)g_RngIndex,
			(unsigned)_v_RngIndex,
			(unsigned)_p_state[_v_RngIndex]
		);

		ImGui::Text(_globals_str.data());

		if (ImGui::TreeNode("Rng State Values"))
		{
			ImGui::PushStyleColor(ImGuiCol_FrameBg, 0x00000000);
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, 0x00000000);
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, 0x00000000);

			if (ImGui::BeginTable("##", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("index", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_None);
				ImGui::TableHeadersRow();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				std::string _state_offset_str;
				const auto back_color = ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg);
				const auto back_color_alt = ImGui::GetStyleColorVec4(ImGuiCol_TableRowBgAlt);

				const auto back_color_active = imvec4_mix(back_color, color_rngval_active);
				const auto back_color_alt_active = imvec4_mix(back_color_alt, color_rngval_active);

				for (uint8_t i = 0; i <= 0xf; i++)
				{
					ImGui::PushID(i);

					if (state_prev[i] ^ _p_state[i])
					{
						state_background_glow_timer[i] = 1.0f;
					}
					state_prev[i] = _p_state[i];

					ImVec4 current_row_bg = back_color;
					ImVec4 current_row_bg_alt = back_color_alt;

					if (i == _v_RngIndex)
					{
						// highlight current value with green
						current_row_bg = back_color_active;
						current_row_bg_alt = back_color_alt_active;
					}
					else if (state_background_glow_timer[i] > 0)
					{
						state_background_glow_timer[i] -= ImGui::GetIO().DeltaTime;

						current_row_bg = imvec4_mix(back_color, color_rngval_update, state_background_glow_timer[i]);
						current_row_bg_alt = imvec4_mix(back_color_alt, color_rngval_update, state_background_glow_timer[i]);
					}

					ImGui::PushStyleColor(ImGuiCol_TableRowBg, current_row_bg);
					ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, current_row_bg_alt);

					ImGui::TableSetColumnIndex(0);
					_state_offset_str = std::format("state[{}]", i);
					ImGui::Text(_state_offset_str.data());

					ImGui::TableSetColumnIndex(1);
					ImGui::InputScalar(
						"##",
						ImGuiDataType_U32,
						&_p_state[i],
						nullptr,
						nullptr,
						"%08x",
						ImGuiInputTextFlags_CharsHexadecimal
					);

					ImGui::TableNextRow();
					
					ImGui::PopStyleColor(2);
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::PopStyleColor(3);

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
	if (ImGui::Begin("Debug Console", &window_console_open))
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
						render_callstack_tooltip(_entry.vm_callstack);
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

void imgui_init()
{
	ImGui::CreateContext();
	ImPlot::CreateContext();

	HWND window = loader_get_window();
	ImGui_ImplWin32_Init(window);

	ImGuiStyle& style = ImGui::GetStyle();
	style.AntiAliasedLines = false;
 	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.058f, 0.058f, 0.058f, window_background_alpha);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03f, 0.03f, 0.03f, window_background_alpha * 1.1);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, window_background_alpha * 1.5);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

void imgui_render()
{
	ImGui::BeginMainMenuBar();
	if (ImGui::BeginMenu("Windows"))
	{
		ImGui::Checkbox("Console", &window_console_open);
		ImGui::Checkbox("Memory Stats", &window_memory_stats_open);
		ImGui::Checkbox("Random Stuff", &window_main_open);
		ImGui::Checkbox("Steamworks Lobbies", &window_lobbies_open);
		ImGui::Checkbox("Instance Browser", &window_instance_browser_open);

		if (ImGui::SliderFloat("Window Background Alpha", &window_background_alpha, 0, 1))
		{
			ImGuiStyle& style = ImGui::GetStyle();
			style.Colors[ImGuiCol_WindowBg] = ImVec4(0.058f, 0.058f, 0.058f, window_background_alpha);
			style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03f, 0.03f, 0.03f, window_background_alpha * 1.1);
			style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, window_background_alpha * 1.5);
		}

		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();

	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	render_memoryusage();
	render_globals();
	render_instance_browser();
	render_instance_details();
	render_dbg_console();
	render_lobby_list();
}