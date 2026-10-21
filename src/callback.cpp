#include "callback.h"

#include "imgui_window.h"

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <implot.h>

#define IS_MOUSE_EVENT(x) (x >= WM_MOUSEFIRST && x <= WM_MOUSELAST)

bool g_uiflag = true;

void handle_wndproc(
	const HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
)
{
	if (uMsg == WM_KEYDOWN && wParam == VK_F3)
	{
		g_uiflag ^= 1;
	}

	if (IS_MOUSE_EVENT(uMsg))
	{
		HCURSOR cursor = LoadCursorA(0, IDC_ARROW);
		SetCursor(cursor);
	}

	LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
}


void dx_present(
	ID3D11RenderTargetView* render_target,
	IDXGISwapChain* swapchain,
	ID3D11Device* d3d_device,
	ID3D11DeviceContext* d3d_device_context
)
{
	if (!ImGui::GetIO().BackendRendererUserData)
	{
		ImGui_ImplDX11_Init(d3d_device, d3d_device_context);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (g_uiflag)
	{
		imgui_render();
	}

	ImGui::Render();

	d3d_device_context->OMSetRenderTargets(1, &render_target, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}