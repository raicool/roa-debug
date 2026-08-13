#include "imgui_window.h"

#include "internal_console.h"
#include "instance.h"
#include "mod.h"
#include "steam_lobby_list.h"
#include "yoyo.h"

#include <loader/d3d11_hook.h>

#include <format>

#define TOOLTIP(x, ...) if (ImGui::IsItemHovered()) ImGui::SetTooltip(x, __VA_ARGS__)

extern ObjectsHash** g_ObjectHash;
extern int** g_RngIndex;
extern int** state;

extern internal_console g_dbgstream;
extern internal_console g_relstream;

extern SIGNATURE_SCRIPT_PERFORM Script_Perform;
extern int __vars_get_names__;

int state_prev[16]{ 0 };

async_steamapi_caller g_steamapi_request;
const std::string window_name = std::format("{}", MOD_NAME);

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

char instance_sel_name[32];
bool instance_selected;
void draw_instances(LinkedList<CInstance>* inst_list)
{
	for (int i = 0; i < inst_list->m_Count; i++)
	{
		CInstance* instance = inst_list->m_First + (sizeof(CInstance) * i);

		if (!instance) break;

		sprintf_s(instance_sel_name, 32, "0x%08x", instance);
		ImGui::Selectable(instance_sel_name, &instance_selected);

		if (instance_selected)
		{
			RValue var_names;
			RValue inst_ptr;
			inst_ptr.valuePointer = (void*)instance;
			inst_ptr.type = GML_TYPE_POINTER;
			DynamicArrayOfRValue args
			{
				1,
				{ &inst_ptr,  }
			};

			Script_Perform(
				__vars_get_names__, 
				nullptr, 
				nullptr, 
				1, 
				&var_names, 
				&args
				);

			loader_log_info("0x{:08x}", var_names.type);
// 			Script_Perform(__self_getter__, g_GlobalInstance, g_GlobalInstance, 0, &result, nullptr);
			instance_selected = false;
		}
	}
}

bool object_treenode(CObjectGM* object)
{
	bool pressed = ImGui::TreeNode(object->m_Name);

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
		}
		ImGui::EndTable();
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
	if (ImGui::Begin("test"))
	{
		for (int i = 0; i < (*g_ObjectHash)->m_count; i++)
		{
			CObjectGM* obj = (*g_ObjectHash)->find_object(i);

			if (!obj)
				break;

			if (obj->m_Instances.m_Count > 0)
			{
				if (object_treenode(obj))
				{
					draw_instances(&obj->m_Instances);

					ImGui::TreePop();
				}
			}
		}
	}
	ImGui::End();

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

		internal_console* current_console = nullptr;
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
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("Windows"))
	{
		ImGui::CheckboxFlags("Debug Console", &windows_flag, WINDOW_DBG_CONSOLE);
		ImGui::CheckboxFlags("Globals", &windows_flag, WINDOW_GLOBALS);
		ImGui::CheckboxFlags("Lobby List", &windows_flag, WINDOW_LOBBY_LIST);

		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	uint32_t _flags = windows_flag;
	for (int i = 0; i <= sizeof(uint32_t); i++)
	{
		uint32_t current_flag = (1 << i);
		if (windows_flag & current_flag)
		{
			switch (current_flag)
			{
			case WINDOW_GLOBALS:
				render_globals();
				break;
			case WINDOW_DBG_CONSOLE:
				render_dbg_console();
				break;
			case WINDOW_LOBBY_LIST:
				render_lobby_list();
				break;
			}
			_flags &= ~current_flag;
		}

		if (_flags == 0)
			break;
	}
}