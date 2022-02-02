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

#ifndef OBS_STUDIO_VSTPLUGIN_H
#define OBS_STUDIO_VSTPLUGIN_H

#define VST_MAX_CHANNELS 8
#define BLOCK_SIZE 512

// Windows uses a proxy process
#ifdef WIN32
	#define AEFFCLIENT 1
#endif

#include <string>
#include <obs-module.h>
#include "aeffectx.h"
#include "vst-plugin-callbacks.hpp"
#include "EditorWidget.h"
#include <thread>
#include <mutex>
#include <memory>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

class EditorWidget;

enum class ChunkType { Bank, Program, Parameter };

class VSTPlugin {
	std::unique_ptr<AEffect> effect;
	obs_source_t *sourceContext;
	std::string pluginPath;

	float **inputs;
	float **outputs;
	bool    is_open;

public:
	AEffect* loadEffect();

	bool saveWasClicked;
	std::string chunkDataBank;
	std::string chunkDataProgram;
	std::string chunkDataParameter;
	std::string chunkDataPath;
	std::unique_ptr<EditorWidget> editorWidget;

private:
	bool effectReady = false;
	bool effectCrashed = false;
	std::mutex effectStatusMutex;

	std::string sourceName;
	std::string filterName;
	char        effectName[64];
	// Remove below... or comment out
	char vendorString[64];

#ifdef __APPLE__
	CFBundleRef bundle = NULL;
#elif WIN32
	HINSTANCE dllHandle = nullptr;
#elif __linux__
	void *soHandle = nullptr;
#endif

	void unloadLibrary();
	bool isPortAvailable(const int32_t port);

	static intptr_t
	hostCallback_static(AEffect *effect, int32_t opcode, int32_t index, intptr_t value, void *ptr, float opt)
	{
		if (effect && effect->user) {
			auto *plugin = static_cast<VSTPlugin *>(effect->user);
			return plugin->hostCallback(effect, opcode, index, value, ptr, opt);
		}

		switch (opcode) {
		case audioMasterVersion:
			return (intptr_t)2400;

		default:
			return 0;
		}
	}

	intptr_t hostCallback(AEffect *effect, int32_t opcode, int32_t index, intptr_t value, void *ptr, float opt);

public:
	VSTPlugin(obs_source_t *sourceContext);
	AEffect *getEffect();
	~VSTPlugin();

	void            send_loadEffectFromPath(std::string path);
	void            loadEffectFromPath(std::string path);
	void            send_unloadEffect();
	void            unloadEffect();
	bool            isEditorOpen();
	bool            hasWindowOpen();
	void            openEditor();
	void            removeEditor();
	void            closeEditor();
	void            hideEditor();
	std::string     getChunk(ChunkType type);
	void            send_setChunk();
	void            setChunk(ChunkType type, std::string & data);
	void            setProgram(const int programNumber);
	int             getProgram();
	void            getSourceNames();
	obs_audio_data *process(struct obs_audio_data *audio);
	bool            openInterfaceWhenActive = false;
	std::string     getPluginPath();
	bool		verifyPluginIntegrity();
	bool            isEffectCrashed() const { return effectCrashed; }

#ifdef WIN32
	static intptr_t win_dispatcher(AEffect* a, int b, int c, intptr_t d, void* e, float f, const size_t ptr_size);
	static void win_setParameter(AEffect* a, int b, float c);
	static float win_getParameter(AEffect* a, int b);
	static void win_processReplacing(AEffect* a, float** b, float** c, int d, int arraySize);
	static void win_updateAEffect(AEffect* a);
#endif
};

#endif // OBS_STUDIO_VSTPLUGIN_H
