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

#include "../headers/EditorWidget.h"

void EditorWidget::buildEffectContainer(AEffect *effect)
{
	WNDCLASSEXW wcex{sizeof(wcex)};

	wcex.lpfnWndProc   = DefWindowProcW;
	wcex.hInstance     = GetModuleHandleW(nullptr);
	wcex.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassExW(&wcex);

	const auto style = WS_CAPTION | WS_THICKFRAME | WS_OVERLAPPEDWINDOW;
	m_hwnd = CreateWindow(wcex.lpszClassName, TEXT(""), style, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);

	MoveWindow(m_hwnd, 0, 0, 300, 300, false);

	effect->dispatcher(effect, effEditOpen, 0, 0, m_hwnd, 0);

	VstRect *vstRect = nullptr;
	effect->dispatcher(effect, effEditGetRect, 0, 0, &vstRect, 0);
	if (vstRect) {
		MoveWindow(m_hwnd, vstRect->left, vstRect->top, vstRect->right, vstRect->bottom, false);
	}
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
	// Some plugins can't resize automatically (like SPAN by Voxengo),
	// so we must resize window manually

	// get pointer to vst effect from window long
	LONG_PTR    wndPtr   = (LONG_PTR)GetWindowLongPtrW(m_hwnd, -21 /*GWLP_USERDATA*/);
	AEffect *   effect   = (AEffect *)(wndPtr);
	VstRect *   rec      = nullptr;
	static RECT PluginRc = {0};
	RECT        winRect  = {0};

	GetWindowRect(m_hwnd, &winRect);
	if (effect) {
		effect->dispatcher(effect, effEditGetRect, 1, 0, &rec, 0);
	}

	// compare window rect with VST Rect
	if (rec) {
		if (PluginRc.bottom != rec->bottom || PluginRc.left != rec->left || PluginRc.right != rec->right ||
		    PluginRc.top != rec->top) {
			PluginRc.bottom = rec->bottom;
			PluginRc.left   = rec->left;
			PluginRc.right  = rec->right;
			PluginRc.top    = rec->top;

			// set rect to our window
			AdjustWindowRectEx(&PluginRc,
							   WS_CAPTION | WS_THICKFRAME | WS_OVERLAPPEDWINDOW,
			                   FALSE,
			                   0);

			// move window to apply pos
			MoveWindow(m_hwnd,
			           winRect.left,
			           winRect.top,
			           PluginRc.right - PluginRc.left,
			           PluginRc.bottom - PluginRc.top,
			           TRUE);
		}
	}
}
