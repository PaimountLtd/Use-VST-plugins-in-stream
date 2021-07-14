/*****************************************************************************
Copyright (C) 2016-2017 by Colin Edwards.
Additional Code Copyright (C) 2016-2017 by c3r1c3 <c3r1c3@nevermindonline.com>

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

#ifndef OBS_STUDIO_EDITORDIALOG_H
#define OBS_STUDIO_EDITORDIALOG_H

//#include <QWidget>
#ifdef __APPLE__
#include <QMacCocoaViewContainer>
#elif WIN32
//#include <QWindow>
#include <Windows.h>
#include <thread>
#include <functional>
#elif __linux__
#include <QWindow>
#include <xcb/xcb.h>
#endif

#include "aeffectx.h"
#include "VSTPlugin.h"

#ifdef WIN32

enum WM_USER_MSG {
	// Start at index user + 5 because some plugins were causing issues when sending invalid
	// messages to the main window
	WM_USER_SET_TITLE = WM_USER + 5,
	WM_USER_SHOW,
	WM_USER_CLOSE,
	WM_USER_LOAD_DLL,
	WM_USER_SETCHUNK,
	WM_USER_HIDE
};

#endif

class VSTPlugin;

class VstRect {

public:
	short top;
	short left;
	short bottom;
	short right;
};

class EditorWidget {

	VSTPlugin *plugin;

#ifdef __APPLE__
	QMacCocoaViewContainer *cocoaViewContainer = NULL;
#elif WIN32
	HWND windowHandle        = NULL;
	std::thread windowWorker;
	bool shutdownWorker      = false;
#elif __linux__

#endif

public:
	EditorWidget(VSTPlugin *plugin);
	void buildEffectContainer(AEffect *effect);
	void closeEvent();
	void handleResizeRequest(int width, int height);
	void show();
	void hide();
	void close();
	void setWindowTitle(const char *title);

	void send_show();
	void send_close();
	void send_hide();
	void send_setWindowTitle(const char *title);
	void send_loadEffectFromPath(std::string path);
	void send_setChunk();
#ifdef WIN32
	void buildEffectContainer_worker();
#endif
};

#endif // OBS_STUDIO_EDITORDIALOG_H
