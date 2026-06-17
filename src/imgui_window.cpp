#include "imgui_window.h"

#include "internal_console.h"
#include "mod.h"
#include "yoyo.h"

#include <loader/d3d11_hook.h>

#include <format>

#define TOOLTIP(x, ...) if (ImGui::IsItemHovered()) ImGui::SetTooltip(x, __VA_ARGS__)

extern Hash<CObjectGM>* g_ObjectHash;
extern int** g_RngIndex;
extern int** state;

extern internal_console g_dbgstream;
extern internal_console g_relstream;

int state_prev[16]{ 0 };

const std::string window_name = std::format("{}", MOD_NAME);

enum
{
	WINDOW_GLOBALS     = (1 << 0),
	WINDOW_DBG_CONSOLE = (1 << 1),

	WINDOW_DEFAULTS = WINDOW_GLOBALS | WINDOW_DBG_CONSOLE
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
			if (ImGui::BeginTabItem("Debug Console"))
			{
				current_console = &g_dbgstream;
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Release Console"))
			{
				current_console = &g_relstream;
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

					// gray text color
					ImGui::PushStyleColor(ImGuiCol_Text, 0x7f7f7fff);
					TOOLTIP("Call Stack:\n------------\n%s", _entry.vm_callstack.c_str());
					ImGui::PopStyleColor();
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
			}
			_flags &= ~current_flag;
		}

		if (_flags == 0)
			break;
	}
}