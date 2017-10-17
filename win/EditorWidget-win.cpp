/*****************************************************************************
Copyright (C) 2016-2017 by Colin Edwards.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include <Dwmapi.h>
#include "../headers/EditorWidget.h"

LRESULT WINAPI EffectWindowProc(
		HWND   hWnd,
		UINT   uMsg,
		WPARAM wParam,
		LPARAM lParam)
{
	VSTPlugin *plugin = (VSTPlugin*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg) {
	case WM_CLOSE: 
		plugin->closeEditor();
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void EditorWidget::buildEffectContainer(AEffect *effect)
{
	WNDCLASSEX wcex{sizeof(wcex)};

	wcex.lpfnWndProc   = EffectWindowProc;
	wcex.hInstance     = GetModuleHandle(0);
	wcex.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassEx(&wcex);

	LONG style = 
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DLGFRAME 
		| WS_POPUP | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;
		
	LONG exStyle = WS_EX_DLGMODALFRAME;

	VstRect *vstRect = nullptr;
	effect->dispatcher(effect, effEditGetRect, 0, 0, &vstRect, 0);

	RECT rect = { 0 };
	
	if (vstRect) {
		rect.left = vstRect->left;
		rect.right = vstRect->right;
		rect.top = vstRect->top;
		rect.bottom = vstRect->bottom;

		AdjustWindowRectEx(&rect, style, false, exStyle);
	}

	m_hwnd = 
	CreateWindowEx(exStyle, wcex.lpszClassName, TEXT(""), style, 
		rect.left, rect.top, rect.right, rect.bottom, 
		nullptr, nullptr, nullptr, nullptr);

	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)plugin);

	/* Despite the use of AdjustWindowRectEx here, the docs lie to us 
	   when they say it will give us the smallest size to accomodate the
	   client area wanted. Since Vista came out, we now have to take into
	   account hidden borders introduced by DWM. This can only be done 
	   *after* we've created the window as well. */
	RECT frame;
	DwmGetWindowAttribute(m_hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(RECT));
	
	RECT border;
	border.left = frame.left - rect.left;
	border.top = frame.top - rect.top;
	border.right = rect.right - frame.right;
	border.bottom = rect.bottom - frame.bottom;
	
	rect.left -= border.left;
	rect.top -= border.top;
	rect.right += border.left + border.right;
	rect.bottom += border.top + border.bottom;

	SetWindowPos(m_hwnd, 0, rect.left, rect.top, rect.right, rect.bottom, 0);
	SetWindowPos(m_hwnd, 0, 0, 0, 0, 0, SWP_NOSIZE);

	effect->dispatcher(effect, effEditOpen, 0, 0, m_hwnd, 0);
}

void EditorWidget::setWindowTitle(const char *title)
{
	TCHAR wstrTitle[256];
	MultiByteToWideChar(CP_UTF8, 0, title, -1, &wstrTitle[0], 256);
	SetWindowText(m_hwnd, &wstrTitle[0]);
}

void EditorWidget::close()
{
	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;
}

void EditorWidget::show()
{
	ShowWindow(m_hwnd, SW_SHOW);
}

void EditorWidget::handleResizeRequest(int width, int height)
{
}
