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

#include <vector>

#include "headers/VSTPlugin.h"

#define CBASE64_IMPLEMENTATION
#include "cbase64.h"
#ifdef WIN32
#include <cstringt.h>
#endif
#include <functional>

VSTPlugin::VSTPlugin(obs_source_t *sourceContext)
        : sourceContext{sourceContext}, effect{nullptr}, is_open{false}, saveWasClicked{false}
{

	int numChannels = VST_MAX_CHANNELS;
	int blocksize   = BLOCK_SIZE;

	inputs  = (float **)malloc(sizeof(float **) * numChannels);
	outputs = (float **)malloc(sizeof(float **) * numChannels);
	for (int channel = 0; channel < numChannels; channel++) {
		inputs[channel]  = (float *)malloc(sizeof(float *) * blocksize);
		outputs[channel] = (float *)malloc(sizeof(float *) * blocksize);
	}
}

VSTPlugin::~VSTPlugin()
{
	int numChannels = VST_MAX_CHANNELS;

	for (int channel = 0; channel < numChannels; channel++) {
		if (inputs[channel]) {
			free(inputs[channel]);
			inputs[channel] = NULL;
		}
		if (outputs[channel]) {
			free(outputs[channel]);
			outputs[channel] = NULL;
		}
	}
	if (inputs) {
		free(inputs);
		inputs = NULL;
	}
	if (outputs) {
		free(outputs);
		outputs = NULL;
	}
}

void VSTPlugin::loadEffectFromPath(std::string path)
{
	if (this->pluginPath.compare(path) != 0) {
		blog(LOG_DEBUG, "VST Plug-in: loadEffectfromPath closing editor first and unloading effect; pluginPath %s != %s", this->pluginPath.c_str(), path.c_str());
		closeEditor();
	}
	
	if (!effect) {
		pluginPath = path;
		blog(LOG_DEBUG, "VST Plug-in: loadEffectFromPath from pluginPath %s ", pluginPath.c_str());

		effect     = loadEffect();

		if (!effect) {
			// TODO: alert user of error
			blog(LOG_WARNING, "VST Plug-in: loadEffectFromPath Can't load effect!");
			return;
		}

		// Check plug-in's magic number
		// If incorrect, then the file either was not loaded properly,
		// is not a real VST plug-in, or is otherwise corrupt.
		if (effect->magic != kEffectMagic) {
			blog(LOG_WARNING, "VST Plug-in: loadEffectFromPath magic number is bad");
			return;
		}

		blog(LOG_DEBUG, "VST Plug-in: loadEffectFromPath get effect pointer: %p", effect);
		effect->dispatcher(effect, effGetEffectName, 0, 0, effectName, 0);
		effect->dispatcher(effect, effGetVendorString, 0, 0, vendorString, 0);

		effect->dispatcher(effect, effOpen, 0, 0, nullptr, 0.0f);

		// Set some default properties
		size_t sampleRate = audio_output_get_sample_rate(obs_get_audio());
		effect->dispatcher(effect, effSetSampleRate, 0, 0, nullptr, sampleRate);
		int blocksize = BLOCK_SIZE;
		effect->dispatcher(effect, effSetBlockSize, 0, blocksize, nullptr, 0.0f);

		effect->dispatcher(effect, effMainsChanged, 0, 1, nullptr, 0);

		effectReady = true;

		if (openInterfaceWhenActive) {
			openEditor();
		}
	}
}

void silenceChannel(float **channelData, int numChannels, long numFrames)
{
	for (int channel = 0; channel < numChannels; ++channel) {
		for (long frame = 0; frame < numFrames; ++frame) {
			channelData[channel][frame] = 0.0f;
		}
	}
}

obs_audio_data *VSTPlugin::process(struct obs_audio_data *audio)
{
	if (!effectStatusMutex.try_lock())
		return audio;

	if (effect && effectReady) {
		uint32_t passes = (audio->frames + BLOCK_SIZE - 1) / BLOCK_SIZE;
		uint32_t extra  = audio->frames % BLOCK_SIZE;
		for (uint32_t pass = 0; pass < passes; pass++) {
			uint32_t frames = pass == passes - 1 && extra ? extra : BLOCK_SIZE;
			silenceChannel(outputs, VST_MAX_CHANNELS, BLOCK_SIZE);

			float *adata[VST_MAX_CHANNELS];
			for (size_t d = 0; d < VST_MAX_CHANNELS; d++) {
				if (audio->data[d] != nullptr) {
					adata[d] = ((float *)audio->data[d]) + (pass * BLOCK_SIZE);
				} else {
					adata[d] = inputs[d];
				}
			};

			effect->processReplacing(effect, adata, outputs, frames);

			for (size_t c = 0; c < VST_MAX_CHANNELS; c++) {
				if (audio->data[c]) {
					for (size_t i = 0; i < frames; i++) {
						adata[c][i] = outputs[c][i];
					}
				}
			}
		}
	}
	effectStatusMutex.unlock();
	return audio;
}

void VSTPlugin::waitDeleteWorker()
{
	if (deleteWorker != nullptr) {
		if (deleteWorker->joinable()) {
			blog(LOG_WARNING, "VST Plug-in: waitDeleteWorker; waiting on deleteWorker");
			deleteWorker->join();
		}

		delete deleteWorker;
		deleteWorker = nullptr;
	}
}

void VSTPlugin::unloadEffect()
{
	waitDeleteWorker();

	effectStatusMutex.lock();
	effectReady = false;

	if (effect) {
		effect->dispatcher(effect, effStopProcess, 0, 0, nullptr, 0);
		effect->dispatcher(effect, effMainsChanged, 0, 0, nullptr, 0);
		effect->dispatcher(effect, effClose, 0, 0, nullptr, 0.0f);
	}

	effect = nullptr;

	unloadLibrary();

	effectStatusMutex.unlock();
}

bool VSTPlugin::isEditorOpen()
{
	return is_open;
	
}

bool VSTPlugin::hasWindowOpen()
{
	if (editorWidget && editorWidget->hiddenWindow) {
		return false;
	}
	return (editorWidget && editorWidget->m_hwnd != 0);
}

void VSTPlugin::openEditor()
{
	is_open = true;
	
	if (!editorWidget) {
		editorWidget = new EditorWidget(this);
		editorWidget->buildEffectContainer();
	}
	blog(LOG_WARNING, "VST Plug-in: openEditor send_show");
	
	editorWidget->send_show();
}

void VSTPlugin::hideEditor()
{
	is_open = false;
	if (editorWidget && editorWidget->m_hwnd != 0) {
		editorWidget->send_hide();
	}
}

void VSTPlugin::removeEditor() {
	is_open = false;
	editorWidget->m_destructing = true;
	if (editorWidget->windowWorker.joinable()) {
		editorWidget->windowWorker.join();
	}

	delete editorWidget;
	editorWidget = nullptr;
}

void VSTPlugin::closeEditor(bool waitDeleteWorkerOnShutdown)
{
	is_open = false;
	if (editorWidget && editorWidget->m_hwnd != 0) {
		blog(LOG_DEBUG,
			"VST Plug-in: closeEditor, editor is open", 
			  effect,
			  editorWidget 
		);

		blog(LOG_WARNING, "VST Plug-in: closeEditor, sending close... and creating new delete worker");
		editorWidget->send_close();

		// Wait the last instance of the delete worker, if any
		waitDeleteWorker();

		deleteWorker = new std::thread(std::bind(&VSTPlugin::removeEditor, this));

		if (waitDeleteWorkerOnShutdown) {
			waitDeleteWorker();
		}
		
	} else {
		blog(LOG_WARNING,
			"VST Plug-in: closeEditor, editor is NOT open"
		);
	}
}

intptr_t VSTPlugin::hostCallback(AEffect *effect, int32_t opcode, int32_t index, intptr_t value, void *ptr, float opt)
{
	UNUSED_PARAMETER(effect);
	UNUSED_PARAMETER(ptr);
	UNUSED_PARAMETER(opt);

	intptr_t result = 0;

	switch (opcode) {
	case audioMasterSizeWindow:
		return 0;
	}

	return result;
}

std::string VSTPlugin::getChunk(ChunkType type)
{
	cbase64_encodestate encoder;
	std::string encodedData;
	blog(LOG_INFO, "VST Plug-in: getChunk started");

	if (!effect) {
		blog(LOG_WARNING, "VST Plug-in: getChunk, no effect loaded");
		return "";
	}

	cbase64_init_encodestate(&encoder);

	if (effect->flags & effFlagsProgramChunks && type != ChunkType::Parameter) {
		void *buf = nullptr;
		intptr_t chunkSize = effect->dispatcher(effect, effGetChunk, type == ChunkType::Bank ? 0 : 1, 0, &buf, 0.0);

		if (!buf || chunkSize==0) {
			blog(LOG_WARNING, "VST Plug-in: effGetChunk failed");
			return "";
		}

		encodedData.resize(cbase64_calc_encoded_length(chunkSize));

		int blockEnd = 
		cbase64_encode_block(
			(const unsigned char*)buf, 
			chunkSize, &encodedData[0], 
			&encoder);

		cbase64_encode_blockend(&encodedData[blockEnd], &encoder);
		blog(LOG_WARNING, "VST Plug-in: getChunk by effGetChunk complete,  %s", encodedData.c_str());
		return encodedData;
	} else if (!(effect->flags & effFlagsProgramChunks) && type == ChunkType::Parameter) {
		std::vector<float> params;
		for (int i = 0; i < effect->numParams; i++) {
			float parameter = effect->getParameter(effect, i);
			params.push_back(parameter);
		}

		const char *bytes = reinterpret_cast<const char *>(&params[0]);
		size_t size = sizeof(float) * params.size();

		encodedData.resize(cbase64_calc_encoded_length(size));

		int blockEnd = 
		cbase64_encode_block(
			(const unsigned char*)bytes, 
			size, &encodedData[0], 
			&encoder);

		cbase64_encode_blockend(&encodedData[blockEnd], &encoder);
		blog(LOG_WARNING, "VST Plug-in: getChunk by getParameter complete,  %s", encodedData.c_str());
		return encodedData;
	}
	blog(LOG_INFO, "VST Plug-in: getChunk option unavailable");
	return "";
}

void VSTPlugin::setChunk(ChunkType type, std::string & data)
{
	if (data.size() == 0) {
		blog(LOG_WARNING, "VST Plug-in: setChunk with empty data chunk ignored");
		return;
	}

	blog(LOG_INFO, "VST Plug-in: setChunk called for data %s", data.c_str());

	cbase64_decodestate decoder;
	cbase64_init_decodestate(&decoder);
	std::string decodedData;

	if (!effect) {
		blog(LOG_WARNING, "VST Plug-in: setChunk effect is not ready yet");
		return;
	}

	if ( this->chunkDataPath != this->pluginPath) {
		blog(LOG_WARNING, "VST Plug-in: setChunk Invalid chunk settings for plugin. Path missmatch.");
		data = "";
		return;
	}

	decodedData.resize(cbase64_calc_decoded_length(data.data(), data.size()));
	cbase64_decode_block(data.data(), data.size(), (unsigned char*)&decodedData[0], &decoder);
	data = "";
	
	if (effect->flags & effFlagsProgramChunks && type != ChunkType::Parameter) {
		auto ret = effect->dispatcher(effect, effSetChunk, type == ChunkType::Bank ? 0 : 1, decodedData.length(), &decodedData[0], 0.0);
		blog(LOG_WARNING, "VST Plug-in: setChunk get %08X from effSetChunk", ret);
	} else if (!(effect->flags & effFlagsProgramChunks) && type == ChunkType::Parameter) {
		const char * p_chars  = &decodedData[0];
		const float *p_floats = reinterpret_cast<const float *>(p_chars);

		int size = decodedData.length() / sizeof(float);

		std::vector<float> params(p_floats, p_floats + size);

		if (params.size() != (size_t)effect->numParams) {
			blog(LOG_WARNING, "VST Plug-in: setChunk wrong number of params");
			return;
		}

		for (int i = 0; i < effect->numParams; i++) {
			effect->setParameter(effect, i, params[i]);
		}
	}
	blog(LOG_WARNING, "VST Plug-in: setChunk finished");
}

void VSTPlugin::setProgram(const int programNumber)
{
	blog(LOG_ERROR, "VST Plug-in: setProgram for %d", programNumber);
	if (programNumber < effect->numPrograms) {
		int ret = effect->dispatcher(effect, effSetProgram, 0, programNumber, NULL, 0.0f);
		blog(LOG_ERROR, "VST Plug-in: setProgram get %d from effSetProgram", ret);
	} else {
		blog(LOG_ERROR, "VST Plug-in: setProgram Failed to load program, number was outside possible program range.");
	}
}

int VSTPlugin::getProgram()
{
	int ret = effect->dispatcher(effect, effGetProgram, 0, 0, NULL, 0.0f);
	blog(LOG_ERROR, "VST Plug-in: getProgram get %d from effGetProgram", ret);
	return ret;
}

void VSTPlugin::getSourceNames()
{
	/* Only call inside the vst_filter_audio function! */
	sourceName = obs_source_get_name(obs_filter_get_target(sourceContext));
	filterName = obs_source_get_name(sourceContext);
}

std::string VSTPlugin::getPluginPath()
{
	return pluginPath;
}
