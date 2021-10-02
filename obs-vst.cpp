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

#include <string>
#include <vector>
#include <algorithm>
#include <set>

#include <util/platform.h>
#include <util/dstr.h>

#ifdef WIN32
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <tchar.h>
#include <ImageHlp.h>
#pragma comment(lib, "Imagehlp.lib")
#endif

#include "headers/VSTPlugin.h"

#define OPEN_VST_SETTINGS "open_vst_settings"
#define CLOSE_VST_SETTINGS "close_vst_settings"
#define OPEN_WHEN_ACTIVE_VST_SETTINGS "open_when_active_vst_settings"
#define SAVE_VST_TEXT obs_module_text("Save")

#define PLUG_IN_NAME obs_module_text("VstPlugin")
#define OPEN_VST_TEXT obs_module_text("OpenPluginInterface")
#define CLOSE_VST_TEXT obs_module_text("ClosePluginInterface")
#define OPEN_WHEN_ACTIVE_VST_TEXT obs_module_text("OpenInterfaceWhenActive")

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-vst", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "VST 2.x Plug-in filter";
}

bool isUpdateFromCreate      = false;
bool isUpdateFromCloseEditor = false;

static void vst_save(void *data, obs_data_t *settings);

static bool open_editor_button_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;

	blog(LOG_WARNING, "VST Plug-in: Open editor btn clicked");

	vstPlugin->openEditor();

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);

	return true;
}

static bool close_editor_button_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	isUpdateFromCloseEditor = true;

	VSTPlugin *vstPlugin = (VSTPlugin *)data;
	blog(LOG_WARNING, "VST Plug-in: Close editor btn clicked");

	vstPlugin->hideEditor();

	UNUSED_PARAMETER(property);
	UNUSED_PARAMETER(props);

	return true;
}

static const char *vst_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return PLUG_IN_NAME;
}

static void vst_destroy(void *data)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;
	vstPlugin->closeEditor(true);
	vstPlugin->unloadEffect();
	delete vstPlugin;
}

static void vst_update(void *data, obs_data_t *settings)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;

	vstPlugin->openInterfaceWhenActive = obs_data_get_bool(settings, OPEN_WHEN_ACTIVE_VST_SETTINGS);
	const char *path                   = obs_data_get_string(settings, "plugin_path");

	if (!path || !strcmp(path, "")) {
		return;
	}

	// Load VST plugin only when creating the filter or when changing plugin
	blog(LOG_WARNING, "VST Plug-in: update settings called from create: %d", isUpdateFromCreate);

	bool load_vst = false;
	if (vstPlugin->getPluginPath().compare(std::string(path)) != 0 || !vstPlugin->hasWindowOpen()) {
		load_vst = true;
	}

	if (load_vst) {
		// unload previous effect only when changing
		if (vstPlugin->getPluginPath().compare(std::string(path)) != 0) {
			vstPlugin->closeEditor();
			vstPlugin->unloadEffect();
		}
		if (!vstPlugin->editorWidget) {
			vstPlugin->editorWidget = new EditorWidget(vstPlugin);
			vstPlugin->editorWidget->buildEffectContainer();
		}
		
		vstPlugin->send_loadEffectFromPath(std::string(path));

		// Load chunk only when creating the filter
		const char *chunkDataBankV3 = obs_data_get_string(settings, "chunk_data_0_v3");
		const char *chunkDataProgramV3 = obs_data_get_string(settings, "chunk_data_1_v3");
		const char *chunkDataPV3 = obs_data_get_string(settings, "chunk_data_p_v3");
		const char *chunkDataPathV3 = obs_data_get_string(settings, "chunk_data_path_v3");

		vstPlugin->chunkDataBank = "";
		vstPlugin->chunkDataProgram = "";
		vstPlugin->chunkDataParameter = "";
		vstPlugin->chunkDataPath = "";
		if (chunkDataPathV3 != NULL && strlen(chunkDataPathV3) > 0) { //check if we have v3
			blog(LOG_DEBUG, "VST Plug-in: Got path from v3 chunk data continue loading v3");
			vstPlugin->chunkDataBank = chunkDataBankV3;
			vstPlugin->chunkDataProgram = chunkDataProgramV3;
			vstPlugin->chunkDataParameter = chunkDataPV3;
			vstPlugin->chunkDataPath = chunkDataPathV3;
		} else {  // migrations from 0.27.1 and from 1.0.0-1.0.3
			const char *chunkDataV2 = obs_data_get_string(settings, "chunk_data_v2");
			if (chunkDataV2 != NULL && strlen(chunkDataV2)>0) { //check if we have v2
				blog(LOG_DEBUG, "VST Plug-in: Got v2 chunk data continue migrating from v2");
				std::string chunkData = chunkDataV2;
				size_t pathPos = chunkData.find_first_of('|');
				if (pathPos == std::string::npos) { //strange not to have path in v2
					vstPlugin->chunkDataPath = path;
					vstPlugin->chunkDataBank = chunkData;
					vstPlugin->chunkDataParameter = vstPlugin->chunkDataBank;
				} else {
					vstPlugin->chunkDataPath = chunkData.substr(0, pathPos);
					size_t lastPos = chunkData.find_last_of('|');
					vstPlugin->chunkDataBank = chunkData.substr(lastPos+1);
					vstPlugin->chunkDataParameter = vstPlugin->chunkDataBank;
				}

				obs_data_set_string(settings, "chunk_data_v2", "");
			} else {
				const char *chunkDataOld = obs_data_get_string(settings, "chunk_data");
				blog(LOG_DEBUG, "VST Plug-in: Got old version chunk data continue migrating from old version");
				if (chunkDataOld != NULL && strlen(chunkDataOld)>0) { // do we have old data
					std::string chunkData = chunkDataOld;
					size_t pathPos = chunkData.find_first_of('|');
					if (pathPos == std::string::npos) {
						vstPlugin->chunkDataPath = path;
						vstPlugin->chunkDataProgram = chunkData;
						vstPlugin->chunkDataParameter = vstPlugin->chunkDataProgram;
					} else {
						vstPlugin->chunkDataPath = chunkData.substr(0, pathPos);
						size_t lastPos = chunkData.find_last_of('|');
						vstPlugin->chunkDataBank = chunkData.substr(lastPos+1);
						vstPlugin->chunkDataParameter = vstPlugin->chunkDataBank;
						obs_data_set_string(settings, "chunk_data", "");
					}
				} else {
					// no data just create empty
				}
			}
		}

		blog(LOG_DEBUG, "VST Plug-in: Loading chunk for filter %s | %s | %s | %s ", vstPlugin->chunkDataBank.c_str(), vstPlugin->chunkDataProgram.c_str(), vstPlugin->chunkDataParameter.c_str(), vstPlugin->chunkDataPath.c_str());

		if (vstPlugin->chunkDataPath.size() > 0 && isUpdateFromCreate) {
			isUpdateFromCreate = false;
			vstPlugin->send_setChunk();
		}
	} else {
		blog(LOG_WARNING, "VST Plug-in: not loading path %s because same path or editor still open", path);
	}

	// Save chunk only after closing the editor

	if (isUpdateFromCloseEditor) {
		blog(LOG_WARNING, "VST Plug-in: update settings from closed editor");
		vst_save(data, settings);
		isUpdateFromCloseEditor = false;
	}
}

static void *vst_create(obs_data_t *settings, obs_source_t *filter)
{
	isUpdateFromCreate = true;

	VSTPlugin *vstPlugin = new VSTPlugin(filter);

	vst_update(vstPlugin, settings);

	return vstPlugin;
}

static void vst_save(void *data, obs_data_t *settings)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;

	obs_data_set_string(settings, "chunk_data_0_v3", vstPlugin->getChunk(ChunkType::Bank).c_str());
	obs_data_set_string(settings, "chunk_data_1_v3", vstPlugin->getChunk(ChunkType::Program).c_str());
	obs_data_set_string(settings, "chunk_data_p_v3", vstPlugin->getChunk(ChunkType::Parameter).c_str());

	const char *path = obs_data_get_string(settings, "plugin_path");
	obs_data_set_string(settings, "chunk_data_path_v3", path);
}

static struct obs_audio_data *vst_filter_audio(void *data, struct obs_audio_data *audio)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;
	vstPlugin->process(audio);

	/*
	 * OBS can only guarantee getting the filter source's parent and own name
	 * in this call, so we grab it and return the results for processing
	 * by the EditorWidget.
	 */
	vstPlugin->getSourceNames();

	return audio;
}

#ifdef WIN32
// Based on https://github.com/processhacker/processhacker/blob/master/phlib/mapimg.c
// Testing this function takes about 1/10 of a ms to complete, pretty fast
void ListModuleDependencies_Uppercase(const std::string& moduleName, std::set<std::string>& output)
{
	output.clear();
	HANDLE fileHandle = CreateFileA(moduleName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

	if (fileHandle == NULL) {
		return;
	}

	DWORD fileSize = GetFileSize(fileHandle, NULL);        
	HANDLE mappingHandle = CreateFileMappingW(fileHandle, NULL, PAGE_READONLY, 0, fileSize, NULL);

	if (mappingHandle == NULL) {
		CloseHandle(fileHandle);
		return;
	}
    
	#define PTR_ADD_OFFSET(Pointer, Offset) ((PVOID)((ULONG_PTR)(Pointer) + (ULONG_PTR)(Offset)))
	#define PH_MAPPED_IMAGE_DELAY_IMPORTS 0x1
	#define PH_MAPPED_IMAGE_DELAY_IMPORTS_V1 0x2

	typedef struct _PH_MAPPED_IMAGE {
	    USHORT Signature{0};
	    PVOID ViewBase{0};
	    SIZE_T Size{0};
	
	    union {
	        struct  {
	            union  {
	                PIMAGE_NT_HEADERS32 NtHeaders32;
	                PIMAGE_NT_HEADERS NtHeaders;
	            };
	
	            ULONG NumberOfSections;
	            PIMAGE_SECTION_HEADER Sections;
	            USHORT Magic;
	        };
	        struct {
	            struct _ELF_IMAGE_HEADER *Header;
	            union  {
	                struct _ELF_IMAGE_HEADER32 *Headers32;
	                struct _ELF_IMAGE_HEADER64 *Headers64;
	            };
	        };
	    };
	} PH_MAPPED_IMAGE, *PPH_MAPPED_IMAGE;
	
	typedef struct _PH_MAPPED_IMAGE_IMPORTS {
	    PPH_MAPPED_IMAGE MappedImage;
	    ULONG Flags;
	    ULONG NumberOfDlls;
	
	    union
	    {
	        PIMAGE_IMPORT_DESCRIPTOR DescriptorTable;
	        PIMAGE_DELAYLOAD_DESCRIPTOR DelayDescriptorTable;
	    };
	} PH_MAPPED_IMAGE_IMPORTS, *PPH_MAPPED_IMAGE_IMPORTS;
	
	typedef struct _PH_MAPPED_IMAGE_IMPORT_DLL {
	    PPH_MAPPED_IMAGE MappedImage;
	    ULONG Flags;
	    PSTR Name;
	    ULONG NumberOfEntries;
	
	    union
	    {
	        PIMAGE_IMPORT_DESCRIPTOR Descriptor;
	        PIMAGE_DELAYLOAD_DESCRIPTOR DelayDescriptor;
	    };
	    PVOID LookupTable;
	} PH_MAPPED_IMAGE_IMPORT_DLL, *PPH_MAPPED_IMAGE_IMPORT_DLL;
	
	auto PhMappedImageRvaToSection = [&](PPH_MAPPED_IMAGE MappedImage, ULONG Rva) {
	    for (ULONG i = 0; i < MappedImage->NumberOfSections; i++)
	    {
	        if ((Rva >= MappedImage->Sections[i].VirtualAddress) && (Rva < MappedImage->Sections[i].VirtualAddress + MappedImage->Sections[i].SizeOfRawData))
	            return &MappedImage->Sections[i];
	    }
	
	    return (PIMAGE_SECTION_HEADER)nullptr;
	};

	auto PhMappedImageRvaToVa = [&](PPH_MAPPED_IMAGE MappedImage, ULONG Rva) {
		PIMAGE_SECTION_HEADER section;
		section = PhMappedImageRvaToSection(MappedImage, Rva);

		if (!section) {
			return (PVOID) nullptr;
		}

		return (PVOID)PTR_ADD_OFFSET(MappedImage->ViewBase, (Rva - section->VirtualAddress) + section->PointerToRawData);
	};

	auto PhMappedImageVaToVa = [&](PPH_MAPPED_IMAGE MappedImage, ULONG Va) {
		#define PTR_SUB_OFFSET(Pointer, Offset) ((PVOID)((ULONG_PTR)(Pointer) - (ULONG_PTR)(Offset)))

		ULONG rva;
		PIMAGE_SECTION_HEADER section;
		
		if (MappedImage->Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		    rva = PtrToUlong(PTR_SUB_OFFSET(Va, MappedImage->NtHeaders32->OptionalHeader.ImageBase));
		else if (MappedImage->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		    rva = PtrToUlong(PTR_SUB_OFFSET(Va, MappedImage->NtHeaders->OptionalHeader.ImageBase));
		else
		    return (PVOID)NULL;
	
		section = PhMappedImageRvaToSection(MappedImage, rva);
	
		if (!section) {
			return (PVOID)NULL;
		}

		return (PVOID)PTR_ADD_OFFSET(MappedImage->ViewBase, PTR_ADD_OFFSET(PTR_SUB_OFFSET(rva, section->VirtualAddress), section->PointerToRawData));
	};

	auto PhProbeAddress = [&](PVOID UserAddress, SIZE_T UserLength, PVOID BufferAddress, SIZE_T BufferLength, ULONG Alignment) {
	    if (UserLength != 0) {
		if (((ULONG_PTR)UserAddress & (Alignment - 1)) != 0) {
			return false;
		}
				 
	        if (((ULONG_PTR)UserAddress + UserLength < (ULONG_PTR)UserAddress) || ((ULONG_PTR)UserAddress < (ULONG_PTR)BufferAddress) || ((ULONG_PTR)UserAddress + UserLength > (ULONG_PTR)BufferAddress + BufferLength))
			return false;
		}

		return true;
	};

	auto PhpMappedImageProbe = [&](PPH_MAPPED_IMAGE MappedImage, PVOID Address, SIZE_T Length) {
		return PhProbeAddress(Address, Length, MappedImage->ViewBase, MappedImage->Size, 1);
	};
    
	PH_MAPPED_IMAGE MappedImage;
	MappedImage.ViewBase = MapViewOfFileEx(mappingHandle, FILE_MAP_READ, 0, 0, fileSize, NULL);
	MappedImage.Size = fileSize;

	do
	{        
		PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)MappedImage.ViewBase;  

		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE || !PhpMappedImageProbe(&MappedImage, dosHeader, sizeof(IMAGE_DOS_HEADER))) {
			break;
		}

		// Get a pointer to the NT headers and probe it.
		ULONG ntHeadersOffset = (ULONG)dosHeader->e_lfanew;

		if (ntHeadersOffset == 0 || ntHeadersOffset >= 0x10000000 || ntHeadersOffset >= fileSize) {
			break;
		}

		MappedImage.NtHeaders = (PIMAGE_NT_HEADERS)PTR_ADD_OFFSET(MappedImage.ViewBase, ntHeadersOffset);
		MappedImage.Magic = MappedImage.NtHeaders->OptionalHeader.Magic;

		if (MappedImage.NtHeaders->Signature != IMAGE_NT_SIGNATURE || MappedImage.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC && MappedImage.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
			break;
		}
        
		// Get a pointer to the first section.
		#define IMAGE_FIRST_SECTION( ntheader ) ((PIMAGE_SECTION_HEADER)        \
		    ((ULONG_PTR)(ntheader) +                                            \
		     FIELD_OFFSET( IMAGE_NT_HEADERS, OptionalHeader ) +                 \
		     ((ntheader))->FileHeader.SizeOfOptionalHeader                      \
		    ))

		MappedImage.NumberOfSections = MappedImage.NtHeaders->FileHeader.NumberOfSections;
		MappedImage.Sections = IMAGE_FIRST_SECTION(MappedImage.NtHeaders);

		// Iterate imports                
		PH_MAPPED_IMAGE_IMPORTS Imports;
		Imports.MappedImage = &MappedImage;
		Imports.Flags = 0;
        
		PIMAGE_DATA_DIRECTORY dataDirectory = nullptr;

		if (MappedImage.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
			PIMAGE_OPTIONAL_HEADER32 optionalHeader = (PIMAGE_OPTIONAL_HEADER32)&MappedImage.NtHeaders->OptionalHeader;

			if (IMAGE_DIRECTORY_ENTRY_IMPORT >= optionalHeader->NumberOfRvaAndSizes) {
				break;
			}

			dataDirectory = &optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		}
		else if (MappedImage.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
			PIMAGE_OPTIONAL_HEADER64 optionalHeader = (PIMAGE_OPTIONAL_HEADER64)&MappedImage.NtHeaders->OptionalHeader;

			if (IMAGE_DIRECTORY_ENTRY_IMPORT >= optionalHeader->NumberOfRvaAndSizes) {
				break;
			}

			dataDirectory = &optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		} else {
			break;
		}

		// Do a scan to determine how many import descriptors there are.
		PIMAGE_IMPORT_DESCRIPTOR descriptor = (PIMAGE_IMPORT_DESCRIPTOR)PhMappedImageRvaToVa(&MappedImage, dataDirectory->VirtualAddress);
		Imports.DescriptorTable = descriptor;
		Imports.NumberOfDlls = 0;

		while (PhpMappedImageProbe(&MappedImage, descriptor, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
			if (descriptor->OriginalFirstThunk == 0 && descriptor->FirstThunk == 0) {
				break;
			}

			descriptor++;
			Imports.NumberOfDlls++;
		}

		for (ULONG Index = 0; Index < Imports.NumberOfDlls; Index++) {
			PH_MAPPED_IMAGE_IMPORT_DLL ImportDll;
			ImportDll.MappedImage = Imports.MappedImage;
			ImportDll.Flags = Imports.Flags;

			if (!(ImportDll.Flags & PH_MAPPED_IMAGE_DELAY_IMPORTS)){
				ImportDll.Descriptor = &Imports.DescriptorTable[Index];
				ImportDll.Name = (PSTR)PhMappedImageRvaToVa(ImportDll.MappedImage, ImportDll.Descriptor->Name);

				if (ImportDll.Name != nullptr) {
					std::string str(ImportDll.Name);
					std::transform(str.begin(), str.end(), str.begin(), std::toupper);
					output.insert(str);
				}
			}
			else {
				ImportDll.DelayDescriptor = &Imports.DelayDescriptorTable[Index];
		
				// Backwards compatible support for legacy V1 delay imports. (dmex)
				if (ImportDll.DelayDescriptor->Attributes.RvaBased == 0) {
					ImportDll.Flags |= PH_MAPPED_IMAGE_DELAY_IMPORTS_V1;
				}
		
				if (!(ImportDll.Flags & PH_MAPPED_IMAGE_DELAY_IMPORTS_V1)) {
					ImportDll.Name = (PSTR)PhMappedImageRvaToVa(ImportDll.MappedImage, ImportDll.DelayDescriptor->DllNameRVA);
				} else {
					ImportDll.Name = (PSTR)PhMappedImageVaToVa(ImportDll.MappedImage, ImportDll.DelayDescriptor->DllNameRVA);
				}
		
				if (ImportDll.Name != nullptr) {
					std::string str(ImportDll.Name);
					std::transform(str.begin(), str.end(), str.begin(), std::toupper);
					output.insert(str);
				}
			}
		}
	}
	while (false);

	UnmapViewOfFile(MappedImage.ViewBase);
	CloseHandle(mappingHandle);
}

void ListExportedFunctions(const std::string& moduleName, std::set<std::string> &output)
{
	output.clear();
	_LOADED_IMAGE LoadedImage;

	// This doesn't load in the sense you're thinking
	if (MapAndLoad(moduleName.c_str(), NULL, &LoadedImage, TRUE, TRUE)) {

		unsigned long cDirSize;
		_IMAGE_EXPORT_DIRECTORY *ImageExportDirectory = (_IMAGE_EXPORT_DIRECTORY *)ImageDirectoryEntryToData(LoadedImage.MappedAddress, false, IMAGE_DIRECTORY_ENTRY_EXPORT, &cDirSize);

		if (ImageExportDirectory != NULL) {
			DWORD *dNameRVAs = (DWORD *)ImageRvaToVa(LoadedImage.FileHeader, LoadedImage.MappedAddress, ImageExportDirectory->AddressOfNames, NULL);

			for (size_t i = 0; i < ImageExportDirectory->NumberOfNames; i++) {
				output.insert((char *)ImageRvaToVa(LoadedImage.FileHeader, LoadedImage.MappedAddress, dNameRVAs[i], NULL));
			}
		}

		UnMapAndLoad(&LoadedImage);
	}
}
#endif

bool is_valid_module(const std::string& fullpath)
{
#ifdef WIN32
	std::set<std::string> deps;
	std::set<std::string> funcs;
	ListModuleDependencies_Uppercase(fullpath, deps);
	ListExportedFunctions(fullpath, funcs);

	// Direct2d and OpenGL are only a part of v3 SDK aka "VSTGUI4"
	// https://steinbergmedia.github.io/vst3_doc/vstgui/html/page_news_and_changes.html
	if (deps.find("OPENGL32.DLL") != deps.end() || deps.find("D2D1.DLL") != deps.end()) {
		blog(LOG_WARNING, "VST Plug-in: excluding %s, VSTGUI4 found", fullpath.c_str());
		return false;
	}

	// These shouldn't be showing up in the list in the first place
	if (funcs.find("VSTPluginMain") == funcs.end() && funcs.find("main") == funcs.end() && funcs.find("VstPluginMain()") == funcs.end()) {
		blog(LOG_WARNING, "VST Plug-in: excluding %s, missing entry point", fullpath.c_str());
		return false;
	}
#endif

	return true;
}

bool valid_extension(const char *filepath)
{
	const char *ext          = os_get_path_extension(filepath);
	int         filters_size = 1;

#ifdef __APPLE__
	const char *filters[] = {".vst"};
#elif WIN32
	const char *             filters[] = {".dll"};
#elif __linux__
	const char *filters[] = {".so", ".o"};
	++filters_size;
#endif

	for (int i = 0; i < filters_size; ++i) {
		if (astrcmpi(filters[i], ext) == 0) {
			return true;
		}
	}

	return false;
}

std::vector<std::string> win32_build_dir_list()
{
	const char *program_files_path = getenv("ProgramFiles");

	const char *dir_list[] = {"/Steinberg/VstPlugins/",
	                          "/Common Files/Steinberg/Shared Components/",
	                          "/Common Files/VST2/",
	                          "/Common Files/VSTPlugins/",
	                          "/VSTPlugins/"};

	const int dir_list_size = sizeof(dir_list) / sizeof(dir_list[0]);

	std::vector<std::string> result(dir_list_size, program_files_path);

	for (int i = 0; i < result.size(); ++i) {
		result[i].append(dir_list[i]);
	}

	return result;
}

typedef std::pair<std::string, std::string> vst_data;

static void find_plugins(std::vector<vst_data> &plugin_list, const char *dir_name)
{
	os_dir_t * dir = os_opendir(dir_name);
	os_dirent *ent = os_readdir(dir);

	while (ent != NULL) {
		std::string path(dir_name);

		if (ent->d_name[0] == '.')
			goto next_entry;

		path.append("/");
		path.append(ent->d_name);

		/* If it's a directory, recurse */
		if (ent->directory) {
			find_plugins(plugin_list, path.c_str());
			goto next_entry;
		}

		/* This works well on Apple since we use *.vst
		 * for the extension but not for other platforms.
		 * A Dll dependency will be added even if it's not
		 * an actual VST plugin. Can't do much about it
		 * unfortunately unless everyone suddenly decided to
		 * use a more sane extension. */
		if (valid_extension(ent->d_name) && is_valid_module(path)) {
			plugin_list.push_back({ent->d_name, path});
		}

	next_entry:
		ent = os_readdir(dir);
	}

	os_closedir(dir);
}

static void fill_out_plugins(obs_property_t *list)
{
#ifdef __APPLE__
	std::vector<std::string> dir_list({"/Library/Audio/Plug-Ins/VST/", "~/Library/Audio/Plug-ins/VST/"});

#elif WIN32
	std::vector<std::string> dir_list  = win32_build_dir_list();

#elif __linux__
	char *vstPathEnv = getenv("VST_PATH");
	if (vstPathEnv != nullptr) {
		std::string dir_list[] = {vstPathEnv};
	} else {
		/* FIXME: Platform dependent areas.
		   Should use environment variables */
		std::vector<std::string> dir_list({"/usr/lib/vst/",
		                                   "/usr/lib/lxvst/",
		                                   "/usr/lib/linux_vst/",
		                                   "/usr/lib64/vst/",
		                                   "/usr/lib64/lxvst/",
		                                   "/usr/lib64/linux_vst/",
		                                   "/usr/local/lib/vst/",
		                                   "/usr/local/lib/lxvst/",
		                                   "/usr/local/lib/linux_vst/",
		                                   "/usr/local/lib64/vst/",
		                                   "/usr/local/lib64/lxvst/",
		                                   "/usr/local/lib64/linux_vst/",
		                                   "~/.vst/",
		                                   "~/.lxvst/"});
	}
#endif

	std::vector<vst_data> vst_list;

	for (int i = 0; i < dir_list.size(); ++i) {
		find_plugins(vst_list, dir_list[i].c_str());
	}

	obs_property_list_add_string(list, "{Please select a plug-in}", nullptr);
	for (int i = 0; i < vst_list.size(); ++i) {
		obs_property_list_add_string(list, vst_list[i].first.c_str(), vst_list[i].second.c_str());
	}
}

static bool open_btn_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)obs_properties_get_param(props);
	if (!vstPlugin) {
		blog(LOG_WARNING, "VST Plug-in:open_btn_changed no VST plugin");
		return true;
	}

	if (vstPlugin->hasWindowOpen()) {
		blog(LOG_WARNING, "VST Plug-in:open_btn_changed has window open");
		obs_property_set_visible(obs_properties_get(props, CLOSE_VST_SETTINGS), true);
	} else {
		blog(LOG_WARNING, "VST Plug-in:open_btn_changed has window closed");
	}
	if (obs_property_is_visible(p) && (vstPlugin->hasWindowOpen() || vstPlugin->isEditorOpen())) {
		obs_property_set_visible(p, false);
		if (!obs_property_is_visible(obs_properties_get(props, CLOSE_VST_SETTINGS))) {
			obs_property_set_visible(obs_properties_get(props, CLOSE_VST_SETTINGS), true);
		}
	}

	return true;
}


static obs_properties_t *vst_properties(void *data)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;

	obs_properties_t *props = obs_properties_create();
	obs_properties_set_param(props, vstPlugin, NULL);

	obs_property_t *list = obs_properties_add_list(
	        props, "plugin_path", PLUG_IN_NAME, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	fill_out_plugins(list);

	obs_property_t *open_button =
	        obs_properties_add_button(props, OPEN_VST_SETTINGS, OPEN_VST_TEXT, open_editor_button_clicked);

	obs_property_t *close_button =
	        obs_properties_add_button(props, CLOSE_VST_SETTINGS, CLOSE_VST_TEXT, close_editor_button_clicked);

	obs_property_set_visible(open_button, true);
	obs_property_set_visible(close_button, false);

	obs_property_set_modified_callback(open_button, open_btn_changed);

	obs_properties_add_bool(props, OPEN_WHEN_ACTIVE_VST_SETTINGS, OPEN_WHEN_ACTIVE_VST_TEXT);

	UNUSED_PARAMETER(data);

	return props;
}

bool obs_module_load(void)
{
	struct obs_source_info vst_filter = {};
	vst_filter.id                     = "vst_filter";
	vst_filter.type                   = OBS_SOURCE_TYPE_FILTER;
	vst_filter.output_flags           = OBS_SOURCE_AUDIO;
	vst_filter.get_name               = vst_name;
	vst_filter.create                 = vst_create;
	vst_filter.destroy                = vst_destroy;
	vst_filter.update                 = vst_update;
	vst_filter.filter_audio           = vst_filter_audio;
	vst_filter.get_properties         = vst_properties;
	vst_filter.save                   = vst_save;

	obs_register_source(&vst_filter);
	return true;
}
