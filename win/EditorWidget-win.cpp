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
#include <gl\gl.h>          // Header File For The OpenGL32 Library
#include <gl\glu.h>         // Header File For The GLu32 Library
//#include <gl\glaux.h>       // Header File For The Glaux Library
#include "../headers/EditorWidget.h"
#include <functional>


LRESULT WINAPI EffectWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd)
		return NULL;

	blog(LOG_WARNING, "EditorWidget: EffectWindowProc, uMsg: %u", uMsg);
	VSTPlugin *plugin = (VSTPlugin *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg) {
	case WM_CLOSE: {
		blog(LOG_WARNING, "EditorWidget: EffectWindowProc, received closeEditor msg");
		plugin->closeEditor();
	}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

GLvoid EditorWidget::ReSizeGLScene(GLsizei width, GLsizei height) // Resize And Initialize The GL Window
{
	if (height == 0) // Prevent A Divide By Zero By
	{
		height = 1; // Making Height Equal One
	}

	glViewport(0, 0, width, height); // Reset The Current Viewport

	glMatrixMode(GL_PROJECTION); // Select The Projection Matrix
	glLoadIdentity();            // Reset The Projection Matrix

	// Calculate The Aspect Ratio Of The Window
	gluPerspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);

	glMatrixMode(GL_MODELVIEW); // Select The Modelview Matrix
	glLoadIdentity();           // Reset The Modelview Matrix
}

int EditorWidget::InitGL(GLvoid) // All Setup For OpenGL Goes Here
{
	glShadeModel(GL_SMOOTH);                           // Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);              // Black Background
	glClearDepth(1.0f);                                // Depth Buffer Setup
	glEnable(GL_DEPTH_TEST);                           // Enables Depth Testing
	glDepthFunc(GL_LEQUAL);                            // The Type Of Depth Testing To Do
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); // Really Nice Perspective Calculations
	return TRUE;                                       // Initialization Went OK
}

int EditorWidget::DrawGLScene(GLvoid) // Here's Where We Do All The Drawing
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear Screen And Depth Buffer
	glLoadIdentity();                                   // Reset The Current Modelview Matrix
	return TRUE;                                        // Everything Went OK
}

HWND EditorWidget::CreateGLWindow(int width, int height, int bits)
{
	GLuint   PixelFormat;             // Holds The Results After Searching For A Match
	WNDCLASSEXW wc{sizeof(wc)};	// Windows Class Structure
	DWORD    dwExStyle;               // Window Extended Style
	DWORD    dwStyle;                 // Window Style
	RECT     WindowRect;              // Grabs Rectangle Upper Left / Lower Right Values
	WindowRect.left   = (long)0;      // Set Left Value To 0
	WindowRect.right  = (long)width;  // Set Right Value To Requested Width
	WindowRect.top    = (long)0;      // Set Top Value To 0
	WindowRect.bottom = (long)height; // Set Bottom Value To Requested Height
	
	HWND      hWnd    = NULL;         // Holds Our Window Handle
	HINSTANCE hInstance;              // Holds The Instance Of The Application

	blog(LOG_WARNING, "EditorWidget: buildEffectContainer_worker");
	MSG msg;
	// create thread message queue
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC ; // Redraw On Size, And Own DC For Window.
	wc.lpfnWndProc   = (WNDPROC)EffectWindowProc;                   // WndProc Handles Messages
	wc.cbClsExtra    = 0;                                  // No Extra Window Data
	wc.cbWndExtra    = 0;                                  // No Extra Window Data
	wc.hInstance     = GetModuleHandleW(nullptr);		 // Set The Instance

	wc.hIcon         = LoadIcon(NULL, IDI_WINLOGO);        // Load The Default Icon
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);        // Load The Arrow Pointer
	wc.hbrBackground = NULL;                               // No Background Required For GL
	wc.lpszMenuName  = NULL;                               // We Don't Want A Menu
	wc.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassExW(&wc); // Set The Class Name

	dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE; // Window Extended Style
	dwStyle   = WS_OVERLAPPEDWINDOW;                // Windows Style

	AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle); // Adjust Window To True Requested Size

	// Create The Window
	if (!(hWnd = CreateWindowEx(dwExStyle,                // Extended Style For The Window
	                            wc.lpszClassName,         // Class Name
	                            TEXT(""),                    // Window Title
	                            dwStyle |                 // Defined Window Style
	                                    WS_CLIPSIBLINGS | // Required Window Style
	                                    WS_CLIPCHILDREN,  // Required Window Style
	                            0,
	                            0,                                  // Window Position
	                            WindowRect.right - WindowRect.left, // Calculate Window Width
	                            WindowRect.bottom - WindowRect.top, // Calculate Window Height
	                            NULL,                               // No Parent Window
	                            NULL,                               // No Menu
	                            wc.hInstance,                     // Instance
	                            NULL)))                             // Dont Pass Anything To WM_CREATE
	{
		KillGLWindow(nullptr); // Reset The Display
		blog(LOG_WARNING, "Could not create windowEx GL Window");
		return FALSE;   // Return FALSE
	}

	static PIXELFORMATDESCRIPTOR pfd = // pfd Tells Windows How We Want Things To Be
	        {
	                sizeof(PIXELFORMATDESCRIPTOR), // Size Of This Pixel Format Descriptor
	                1,                             // Version Number
	                PFD_DRAW_TO_WINDOW |           // Format Must Support Window
	                        PFD_SUPPORT_OPENGL |   // Format Must Support OpenGL
	                        PFD_DOUBLEBUFFER,      // Must Support Double Buffering
	                PFD_TYPE_RGBA,                 // Request An RGBA Format
	                bits,                          // Select Our Color Depth
	                0,
	                0,
	                0,
	                0,
	                0,
	                0, // Color Bits Ignored
	                0, // No Alpha Buffer
	                0, // Shift Bit Ignored
	                0, // No Accumulation Buffer
	                0,
	                0,
	                0,
	                0,              // Accumulation Bits Ignored
	                16,             // 16Bit Z-Buffer (Depth Buffer)
	                0,              // No Stencil Buffer
	                0,              // No Auxiliary Buffer
	                PFD_MAIN_PLANE, // Main Drawing Layer
	                0,              // Reserved
	                0,
	                0,
	                0 // Layer Masks Ignored
	        };

	if (!(hDC = GetDC(hWnd))) // Did We Get A Device Context?
	{
		KillGLWindow(hWnd); // Reset The Display
		blog(LOG_WARNING, "Can't Create A GL Device Context.");
		return FALSE; // Return FALSE
	}

	if (!(PixelFormat = ChoosePixelFormat(hDC, &pfd))) // Did Windows Find A Matching Pixel Format?
	{
		KillGLWindow(hWnd); // Reset The Display
		blog(LOG_WARNING, "Can't Find A Suitable PixelFormat.");
		return FALSE; // Return FALSE
	}

	if (!SetPixelFormat(hDC, PixelFormat, &pfd)) // Are We Able To Set The Pixel Format?
	{
		KillGLWindow(hWnd); // Reset The Display
		blog(LOG_WARNING, "Can't Set The PixelFormat.");
		return FALSE; // Return FALSE
	}

	if (!(hRC = wglCreateContext(hDC))) // Are We Able To Get A Rendering Context?
	{
		KillGLWindow(hWnd); // Reset The Display
		blog(LOG_WARNING, "Can't Create A GL Rendering Context.");
		return FALSE; // Return FALSE
	}

	if (!wglMakeCurrent(hDC, hRC)) // Try To Activate The Rendering Context
	{
		KillGLWindow(hWnd); // Reset The Display
		blog(LOG_WARNING, "Can't Activate The GL Rendering Context.");
		return FALSE; // Return FALSE
	}

	ShowWindow(hWnd, SW_SHOW);    // Show The Window
	SetForegroundWindow(hWnd);    // Slightly Higher Priority
	SetFocus(hWnd);               // Sets Keyboard Focus To The Window
	ReSizeGLScene(width, height); // Set Up Our Perspective GL Screen

	if (!InitGL()) // Initialize Our Newly Created GL Window
	{
		KillGLWindow(hWnd); // Reset The Display
		blog(LOG_WARNING, "InitGL failed");
		return nullptr; // Return FALSE
	}

	blog(LOG_WARNING, "Success create GL window: %p", hWnd);

	return hWnd; // Success
}


GLvoid EditorWidget::KillGLWindow(HWND hWnd) // Properly Kill The Window
{
	if (hRC) // Do We Have A Rendering Context?
	{
		if (!wglMakeCurrent(NULL, NULL)) // Are We Able To Release The DC And RC Contexts?
		{
			blog(LOG_WARNING, "Release of DC and RC failed");
		}

		if (!wglDeleteContext(hRC)) // Are We Able To Delete The RC?
		{
			blog(LOG_WARNING, "Release Rendering Context Failed.");
			
		}
		hRC = NULL; // Set RC To NULL
	}

	if (hDC && hWnd && !ReleaseDC(hWnd, hDC)) // Are We Able To Release The DC
	{
		blog(LOG_WARNING, "Release device context failed");		
		hDC = NULL; // Set DC To NULL
	}

	if (hWnd && !DestroyWindow(hWnd)) // Are We Able To Destroy The Window?
	{
		blog(LOG_WARNING, "Could not release hwnd");
		hWnd = NULL; // Set hWnd To NULL
	}
	/*
	if (!UnregisterClass("OpenGL", hInstance)) // Are We Able To Unregister Class
	{
		MessageBox(NULL, "Could Not Unregister Class.", "SHUTDOWN ERROR", MB_OK | MB_ICONINFORMATION);
		hInstance = NULL; // Set hInstance To NULL
	}*/
}


EditorWidget::~EditorWidget()
{
}	

void EditorWidget::buildEffectContainer(AEffect *effect)
{
	m_effect     = effect;
	blog(LOG_WARNING, "EditorWidget: buildEffectContainer, about to start thread");
	m_threadStarted  = CreateEvent(NULL,              // default security attributes
	              TRUE,              // manual-reset event
	              FALSE,             // initial state is nonsignaled
	              TEXT("ThreadStart") // object name
	); 

	windowWorker = std::thread(std::bind(&EditorWidget::buildEffectContainer_worker, this));
}

void EditorWidget::buildEffectContainer_worker()
{
	WNDCLASSEXW wcex{sizeof(wcex)};
	blog(LOG_WARNING,
		"EditorWidget: buildEffectContainer_worker"
	);
	MSG msg;
	//create thread message queue
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	VstRect *vstRect = nullptr;

	/*
	wcex.lpfnWndProc   = EffectWindowProc;
	wcex.hInstance     = GetModuleHandleW(nullptr);
	wcex.lpszClassName = L"Minimal VST host - Guest VST Window Frame";
	RegisterClassExW(&wcex);

	LONG style =
	        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DLGFRAME | WS_POPUP | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX;

	LONG exStyle = WS_EX_DLGMODALFRAME;

	m_hwnd = CreateWindowEx(
	        exStyle, wcex.lpszClassName, TEXT(""), style, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
	*/

	m_hwnd = CreateGLWindow(600, 600, 24);
	
	blog(LOG_WARNING, "EditorWidget::buildEffectContainer_worker CreateWindowEx, m_hwnd: %p, m_hwn_parent: %p", m_hwnd, m_hwnd_parent);
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)plugin);
	blog(LOG_WARNING, "EditorWidget::m_effect->dispatcher addr: %p", m_effect->dispatcher);

	m_effect->dispatcher(m_effect, effEditIdle, 0, 0, m_hwnd, 0);
	m_effect->dispatcher(m_effect, effEditOpen, 0, 0, m_hwnd, 0);
	//

	blog(LOG_WARNING, "EditorWidget after effEditOpen");

	m_effect->dispatcher(m_effect, effEditGetRect, 0, 0, &vstRect, 0);
	blog(LOG_WARNING, "EditorWidget after effEditGetRect");


	blog(LOG_WARNING, "EditorWidget::buildEffectContainer_worker before show");
	show();
	if (vstRect) {
		SetWindowPos(m_hwnd, 0, vstRect->left, vstRect->top, vstRect->right, vstRect->bottom, 0);
	}

	RECT rect   = {0};
	RECT border = {0};

	if (vstRect) {
		RECT clientRect;

		GetWindowRect(m_hwnd, &rect);
		GetClientRect(m_hwnd, &clientRect);

		border.left  = clientRect.left - rect.left;
		border.right = rect.right - clientRect.right;

		border.top    = clientRect.top - rect.top;
		border.bottom = rect.bottom - clientRect.bottom;
	}

	/* Despite the use of AdjustWindowRectEx here, the docs lie to us
	   when they say it will give us the smallest size to accomodate the
	   client area wanted. Since Vista came out, we now have to take into
	   account hidden borders introduced by DWM. This can only be done
	   *after* we've created the window as well. */
	rect.left -= border.left;
	rect.top -= border.top;
	rect.right += border.left + border.right;
	rect.bottom += border.top + border.bottom;

	SetWindowPos(m_hwnd, 0, rect.left, rect.top, rect.right, rect.bottom, 0);
	SetWindowPos(m_hwnd, 0, 0, 0, 0, 0, SWP_NOSIZE);

	

	BOOL bRet;

	bool shutdown = false;
	if (!SetEvent(m_threadStarted)) {
		blog(LOG_WARNING, "Set Event Failed");
	}
	while (!shutdown && (bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1) {
			blog(LOG_WARNING, "Exit GetMessage loop");
			break;
		} else {
			if (msg.message == WM_USER_SET_TITLE) {
				const char *title = reinterpret_cast<const char *>(msg.wParam);
				setWindowTitle(title);
			} else if (msg.message == WM_USER_SHOW) {
				show();
			} else if (msg.message == WM_USER_CLOSE) {
				close();
				dispatcherClose();
				shutdown = true;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

	}
	return;
}

void EditorWidget::setWindowTitle(const char *title)
{
	TCHAR wstrTitle[256];
	MultiByteToWideChar(CP_UTF8, 0, title, -1, &wstrTitle[0], 256);
	SetWindowText(m_hwnd, &wstrTitle[0]);
}

void EditorWidget::send_setWindowTitle(const char *title)
{
	blog(LOG_WARNING, "EditorWidget::send_setWindowTitle , title: %s", title);
	DWORD res = WaitForSingleObject(m_threadStarted, // event handle
	                                INFINITE);       // indefinite wait
	if (res != WAIT_OBJECT_0) {
		blog(LOG_WARNING, "EditorWidget::send_show WaitForSingeObject failed: %ul", res);
		return;
	}
	BOOL retMsg = PostThreadMessage(GetThreadId(windowWorker.native_handle()),
			  WM_USER_SET_TITLE,
	                  reinterpret_cast<WPARAM>(title),
	                  0);
	if (!retMsg) {
		blog(LOG_WARNING, "EditorWidget::send_setWindowTitle postthreadMessage failed");
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget::send_setWindowTitle, getLastError: %lu", dw);
	}

}

void EditorWidget::close()
{
	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;
}

void EditorWidget::send_close()
{
	sync_data sd;
	blog(LOG_WARNING, "EditorWidget::send_close");

	DWORD res = WaitForSingleObject(m_threadStarted, // event handle
	                                   INFINITE);    // indefinite wait
	if (res != WAIT_OBJECT_0) {
		blog(LOG_WARNING, "EditorWidget::send_close WaitForSingelObject failed: %ul", res);
		return;
	}
	BOOL retMsg = PostThreadMessage(GetThreadId(windowWorker.native_handle()), WM_USER_CLOSE, 0, reinterpret_cast<WPARAM>(&sd));
	if (!retMsg) {
		blog(LOG_WARNING, "EditorWidget::send_close postthreadMessage failed");
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget::send_close, getLastError: %lu", dw);
	}
}

void EditorWidget::show()
{
	blog(LOG_WARNING, "EditorWidget::show Window");
	ShowWindow(m_hwnd, SW_SHOW);
}

void EditorWidget::send_show()
{
	blog(LOG_WARNING, "EditorWidget::send_show");

	DWORD res = WaitForSingleObject(m_threadStarted, // event handle
	                                INFINITE);       // indefinite wait
	if (res != WAIT_OBJECT_0) {
		blog(LOG_WARNING, "EditorWidget::send_show WaitForSingeObject failed: %ul", res);
		return;
	}
	BOOL retMsg = PostThreadMessage(GetThreadId(windowWorker.native_handle()),
		WM_USER_SHOW, 0, 0);
	if (!retMsg) {
		blog(LOG_WARNING, "EditorWidget::send_show postthreadMessage failed");
		DWORD dw = GetLastError();
		blog(LOG_WARNING, "EditorWidget::send_show, getLastError: %lu", dw);
	}
}

void EditorWidget::dispatcherClose()
{
	if (m_effect) {
		m_effect->dispatcher(m_effect, effEditClose, 0, 0, nullptr, 0);
	}
}
