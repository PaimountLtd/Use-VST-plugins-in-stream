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
	WNDCLASSEX wcex{sizeof(wcex)};

	wcex.lpfnWndProc   = DefWindowProc;
	wcex.hInstance     = GetModuleHandle(0);
	wcex.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassEx(&wcex);

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
	// We don't have to do anything here as far as I can tell.
	// The widget will resize the HWIND itself and then this
	// widget will automatically size depending on that.
}
