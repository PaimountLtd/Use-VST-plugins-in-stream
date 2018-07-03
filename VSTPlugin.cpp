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

VSTPlugin::VSTPlugin(obs_source_t *sourceContext) : sourceContext{sourceContext}
{
	int numChannels = VST_MAX_CHANNELS;
	int blocksize   = BLOCK_SIZE;

	inputs  = (float **)malloc(sizeof(float **) * numChannels);
	outputs = (float **)malloc(sizeof(float **) * numChannels);
	for (int channel = 0; channel < numChannels; channel++) {
		inputs[channel]  = (float *)malloc(sizeof(float *) * blocksize);
		outputs[channel] = (float *)malloc(sizeof(float *) * blocksize);
	}

	widget.setWindowCloseCallback(editorWidgetClosed, this);
}

void VSTPlugin::loadEffectFromPath(std::string path)
{
	if (this->libraryPath.compare(path) != 0) {
		closeEditor();
		unloadEffect();
	}

	if (!effect) {
		libraryPath = path;
		effect      = loadLibrary();

		if (!effect) {
			// TODO: alert user of error
			blog(LOG_WARNING,
			     "VST Plug-in: Can't load "
			     "effect!");
			return;
		}

		// Check plug-in's magic number
		// If incorrect, then the file either was not loaded properly,
		// is not a real VST plug-in, or is otherwise corrupt.
		if (effect->magic != kEffectMagic) {
			blog(LOG_WARNING, "VST Plug-in's magic number is bad");
			return;
		}

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

	if (effect && effectReady) {
		silenceChannel(outputs, VST_MAX_CHANNELS, audio->frames);

		float *adata[VST_MAX_CHANNELS];
		for (size_t d = 0; d < VST_MAX_CHANNELS; d++) {
			if (audio->data[d] != NULL) {
				adata[d] = (float *)audio->data[d];
			} else {
				adata[d] = inputs[d];
			}
		};

		effect->processReplacing(effect, adata, outputs, audio->frames);

		for (size_t c = 0; c < VST_MAX_CHANNELS; c++) {
			if (audio->data[c]) {
				for (size_t i = 0; i < audio->frames; i++) {
					adata[c][i] = outputs[c][i];
				}
			}
		}
	}

	return audio;
}

void VSTPlugin::unloadEffect()
{
	effectReady = false;

	if (effect) {
		effect->dispatcher(effect, effMainsChanged, 0, 0, nullptr, 0);
		effect->dispatcher(effect, effClose, 0, 0, nullptr, 0.0f);
	}

	effect = nullptr;

	unloadLibrary();
}

bool VSTPlugin::isEditorOpen()
{
	return widget.isVisible();
}

void VSTPlugin::openEditor()
{
	if (effect) {
		widget.setWindowContent(effect);
		widget.setWindowTitle(effectName);
		widget.show();
	}
}

void VSTPlugin::closeEditor()
{
	if (effect) {
		widget.clearWindowContent(effect);
	}
	if (widget.isVisible()) {
		widget.show(false);
	}
}

intptr_t VSTPlugin::hostCallback(AEffect *effect, int32_t opcode, int32_t index, intptr_t value, void *ptr, float opt)
{
	UNUSED_PARAMETER(effect);
	UNUSED_PARAMETER(ptr);
	UNUSED_PARAMETER(opt);

	intptr_t result = 0;

	// Filter idle calls...
	bool filtered = false;
	if (opcode == audioMasterIdle) {
		static bool wasIdle = false;
		if (wasIdle)
			filtered = true;
		else {
			blog(LOG_WARNING,
			     "VST Plug-in: Future idle calls "
			     "will not be displayed!");
			wasIdle = true;
		}
	}

	switch (opcode) {
	case audioMasterSizeWindow:
		// index: width, value: height
		widget.handleResizeRequest(index, value);
		return 0;
	}

	return result;
}

void VSTPlugin::editorWidgetClosed(void *ptr)
{
	
	reinterpret_cast<VSTPlugin *>(ptr)->closeEditor();
}

std::string VSTPlugin::getChunk()
{
	cbase64_encodestate encoder;
	std::string         encodedData;

	cbase64_init_encodestate(&encoder);

	if (!effect) {
		return "";
	}

	if (effect->flags & effFlagsProgramChunks) {
		void *   buf       = nullptr;
		intptr_t chunkSize = effect->dispatcher(effect, effGetChunk, 1, 0, &buf, 0.0);

		encodedData.resize(cbase64_calc_encoded_length(chunkSize));

		int blockEnd = cbase64_encode_block((const unsigned char *)buf, chunkSize, &encodedData[0], &encoder);

		cbase64_encode_blockend(&encodedData[blockEnd], &encoder);

		return encodedData;
	} else {
		std::vector<float> params;
		for (int i = 0; i < effect->numParams; i++) {
			float parameter = effect->getParameter(effect, i);
			params.push_back(parameter);
		}

		const char *bytes = reinterpret_cast<const char *>(&params[0]);
		size_t      size  = sizeof(float) * params.size();

		encodedData.resize(cbase64_calc_encoded_length(size));

		int blockEnd = cbase64_encode_block((const unsigned char *)bytes, size, &encodedData[0], &encoder);

		cbase64_encode_blockend(&encodedData[blockEnd], &encoder);
		return encodedData;
	}
}

void VSTPlugin::setChunk(std::string data)
{
	cbase64_decodestate decoder;
	cbase64_init_decodestate(&decoder);
	std::string decodedData;

	decodedData.resize(cbase64_calc_decoded_length(data.data(), data.size()));
	cbase64_decode_block(data.data(), data.size(), (unsigned char *)&decodedData[0], &decoder);

	if (!effect) {
		return;
	}

	if (effect->flags & effFlagsProgramChunks) {
		effect->dispatcher(effect, effSetChunk, 1, decodedData.length(), &decodedData[0], 0);
	} else {
		const char * p_chars  = &decodedData[0];
		const float *p_floats = reinterpret_cast<const float *>(p_chars);

		int size = decodedData.length() / sizeof(float);

		std::vector<float> params(p_floats, p_floats + size);

		if (params.size() != effect->numParams) {
			return;
		}

		for (int i = 0; i < effect->numParams; i++) {
			effect->setParameter(effect, i, params[i]);
		}
	}
}

void VSTPlugin::setProgram(const int programNumber)
{
	if (programNumber < effect->numPrograms) {
		effect->dispatcher(effect, effSetProgram, 0, programNumber, NULL, 0.0f);
	} else {
		blog(LOG_ERROR, "Failed to load program, number was outside possible program range.");
	}
}

int VSTPlugin::getProgram()
{
	return effect->dispatcher(effect, effGetProgram, 0, 0, NULL, 0.0f);
}
