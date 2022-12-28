#include <string>
#include <queue>
#include "Hooking.hpp"
#include "Game.hpp"
#include "ControlHooks.hpp"
#include "Camera.hpp"

#include "game/reloc.hpp"
#include "game/d3d/d3dterrain.hpp"
#include "game/script/script.hpp"
#include "game/font.hpp"

LPDIRECT3DDEVICE9 pDevice;
HWND pHwnd;

Hooking::Hooking()
	: m_menu(nullptr)
{
	// load config.json into m_config
	LoadConfig();

#if ROTTR
	ReadModFileList();

	auto pDLCSystem_Create = FindPattern((PBYTE)"\x48\x83\xEC\x28\x48\x83\x3D\x00\x00\x00\x00\x00\x75\x2E\xB9\x90\x01", "xxxxxxx?????xxxxx");
	MH_CreateHook(reinterpret_cast<void*>(pDLCSystem_Create), DLCSystem_Create, (void**)&orgDLCSystem_Create);
	MH_EnableHook(DLCSystem_Create);

	auto pTigerAFS_RequestRead = FindPattern((PBYTE)"\x48\x89\x5C\x24\x18\x56\x41\x56\x41\x57\x48\x83\xEC\x20\x83\x79\x34\x00", "xxxxxxxxxxxxxxxxxx");
	MH_CreateHook(reinterpret_cast<void*>(pTigerAFS_RequestRead), TigerArchiveFileSystem_RequestRead, (void**)&orgTigerArchiveFileSystem_RequestRead);
	MH_EnableHook(TigerArchiveFileSystem_RequestRead);

	auto pResolveReceiver_ReceiveData = FindPattern((PBYTE)"\x48\x89\x5C\x24\x08\x44\x89\x4C\x24\x20\x4C\x89\x44\x24\x18\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D"
		, "xxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	MH_CreateHook(reinterpret_cast<void*>(pResolveReceiver_ReceiveData), ReceiveData, (void**)&orgReceiveData);
	MH_EnableHook(ReceiveData);
#else
	// hook into d3d9 creation function and wait for a device
#if TRAE
	auto pFound = FindPattern((PBYTE)"\xE8\x00\x00\x00\x00\x85\xC0\x75\x00\xE8\x00\x00\x00\x00\x6A\x00\x6A\x22\xE8", "x????xxx?x????xxxxx");
	auto D3D_Init = GetAddress(pFound, 1, 5);

	MH_CreateHook(reinterpret_cast<void*>(D3D_Init), hooked_Direct3DInit, reinterpret_cast<void**>(&original_Direct3DInit));
#elif TR8
	MH_CreateHook(reinterpret_cast<void*>(0x478640), hooked_Direct3DInit, reinterpret_cast<void**>(&original_Direct3DInit));
#elif TR7
	auto pFound = FindPattern((PBYTE)"\xE8\x00\x00\x00\x00\x8B\x00\x89\x00\xD4\x85\x00", "x????x?x?xx?");
	assert(pFound);

	auto D3D_Init = GetAddress(pFound, 1, 5);
	MH_CreateHook(reinterpret_cast<void*>(D3D_Init), hooked_Direct3DInit, reinterpret_cast<void**>(&original_Direct3DInit));
#endif

	// remove intros
	// TODO this code make quite a lot of assumptions, refactor
	if (m_config.remove_legal_screen)
	{
#if TRAE
		NOP((void*)0x0045FDBA, 10);
		NOP((void*)0x0045FDCE, 6);
		NOP((void*)0x0045FD3F, 6);

		*(int*)0x838838 = 3;
#elif TR7 && RETAIL_VERSION
		NOP((void*)0x45D043, 10);
		NOP((void*)0x45D057, 6);
		NOP((void*)0x45CFC9, 6);

		*(int*)0x10E5868 = 3;
#endif
	}

	InstallControlHooks();
	InstallCameraHooks();
	InsertTerrainDrawableHooks();

	FONT_Init();

#if TRAE
	MH_CreateHook((void*)0x467E60, MakePeHandle, nullptr);
#elif TR7
	MH_CreateHook((void*)ADDR(0x467310, 0x4642F0), MakePeHandle, nullptr);
#endif

#if TR8
	MultiFileSystem_Add = reinterpret_cast<void(__thiscall*)(void*, cdc::FileSystem*, bool, bool)>(0x472C50);
#endif

	Game::Initialize();
#endif

	MH_EnableHook(MH_ALL_HOOKS);
}

Hooking::~Hooking()
{
	MH_Uninitialize();
}

std::shared_ptr<Menu>& Hooking::GetMenu() noexcept
{
	return m_menu;
}

char(__thiscall* original__PCDeviceManager__CreateDevice)(DWORD* _this, DWORD a2);

char __fastcall PCDeviceManager__CreateDevice(DWORD* _this, DWORD _, DWORD a2)
{
	auto val = original__PCDeviceManager__CreateDevice(_this, a2);

	if (!val)
	{
		return val;
	}

#if TRAE
	auto address = *reinterpret_cast<DWORD*>(0xA6669C);
#elif TR8
	auto address = *reinterpret_cast<DWORD*>(0xAD75E4);
#elif TR7
	auto address = *reinterpret_cast<DWORD*>(ADDR(0x139C758, 0x1392E18));
#elif ROTTR
	auto address = *reinterpret_cast<DWORD*>(0xDEAD0000); // dummy address for now
#endif
	pDevice = *reinterpret_cast<IDirect3DDevice9**>(address + 0x20);

	Hooking::GetInstance().GetMenu()->SetDevice(pDevice);

	return val;
}

void(__thiscall* orginal_PCDeviceManager__ReleaseDevice)(DWORD* _this, int status);

void __fastcall PCDeviceManager__ReleaseDevice(DWORD* _this, DWORD _, int status)
{
	Hooking::GetInstance().GetMenu()->OnDeviceReleased();
	ImGui_ImplDX9_InvalidateDeviceObjects();
	
	pDevice = nullptr;

	orginal_PCDeviceManager__ReleaseDevice(_this, status);
}

float* (__cdecl* TRANS_RotTransPersVectorf)(DWORD a1, DWORD a2);
void(__cdecl* org_Font__Flush)();

cdc::FileSystem* GetFS()
{
#ifndef TR8
	static cdc::FileSystem* pFS = CreateMultiFileSystem(*(cdc::FileSystem**)ARCHIVEFS, *(cdc::FileSystem**)DISKFS);

	return pFS;
#else
	// return regular FS in Underworld since this function is used by our code too
	return g_pFS;
#endif
}

#if ROTTR
ModMap g_ModFileList;
unsigned long g_Update1Size; // will append mods at the end of update1 tiger file

void(__cdecl* orgDLCSystem_Create)(void);
void* (__cdecl* orgTigerArchiveFileSystem_RequestRead)(void* _this, void* receiver, const char* fileName, unsigned int startOffset);
int(__cdecl* orgReceiveData)(cdc::ResolveReceiver* _this, void* fileRequest, char* param_1, int param_2, unsigned int param_3);


void AddModEntry(const char* path, char* fn, unsigned long nFileSizeLow)
{
	char* p = fn;                   // section file name is in the form of <type+hash>_<uncompressed size>.<ext>
	while (*p && isxdigit(*p))
		p++;
	if ((p - fn) > 0) // has hex number at start of file name
	{
		char* num_end;
		unsigned long hash = strtoul(fn, &num_end, 16);
		if (*num_end == '_') // delimiter for file size
		{
			unsigned long uncompressed_size = strtoul(num_end + 1, nullptr, 16);  // get uncompress file size

			unsigned long compressed_size = nFileSizeLow; // compressed file size
			std::string fullpath = path;
			fullpath += fn;
			g_ModFileList[hash] = ModInfo(fullpath.c_str(), compressed_size, uncompressed_size);
		}
	}
}

void ReadModFileList()
{
	WIN32_FIND_DATAA file;
	char* ModDir = "Mods\\";
	std::string fmask = "*.*";
	std::vector<std::wstring> vs;
	HANDLE hFind;

	// get file size of update1.000.tiger
	struct stat stat_buf;
	int rc = stat("bigfile.update1.000.000.tiger", &stat_buf);
	g_Update1Size = stat_buf.st_size;

	std::priority_queue<std::string, std::vector<std::string>,
		std::greater<std::string> > dirs;
	dirs.push(ModDir); // start with passed directory 

	do {
		std::string path = dirs.top();// retrieve directory to search
		dirs.pop();

		if (path[path.size() - 1] != '\\')  // normalize the name.
			path += "\\";

		std::string mask = path + fmask;    // create mask for searching

		// First search for files:
		if (INVALID_HANDLE_VALUE == (hFind = FindFirstFileA(mask.c_str(), &file)))
			continue;

		do {
			if (!(file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				AddModEntry(path.c_str(), file.cFileName, file.nFileSizeLow);
		} while (FindNextFileA(hFind, &file));
		FindClose(hFind);
		// Then search for subdirectories:
		if (INVALID_HANDLE_VALUE == (hFind = FindFirstFileA((path + "*").c_str(), &file)))
		continue;
		do {
			if ((file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (file.cFileName[0] != '.'))
				dirs.push(path + file.cFileName);
		} while (FindNextFileA(hFind, &file));
		FindClose(hFind);
	} while (!dirs.empty());
}

typedef std::map<uint32_t, uint32_t> offset2hash;

offset2hash offset_map; // map file offset to section hash id

cdc::FileSystem* g_pOrigFS = NULL;


void DLCSystem_Create(void)
{
	orgDLCSystem_Create();
	//  get g_pDISKFS pointer after it is initialzed
	auto pFound = FindPattern((PBYTE)"\x48\x8B\x15\x00\x00\x00\x00\x4C\x8D\x05\x00\x00\x00\x00\x48\x8B\xC8", "xxx????xxx????xxx");
	uint64_t* pDiskFS = (uint64_t*) GetAddress(pFound, 3, 7);
	g_pOrigFS = (cdc::FileSystem*)*pDiskFS;
}

void* TigerArchiveFileSystem_RequestRead(void* _this, void* receiver, const char* fileName, unsigned int startOffset)
{
	printf("TigerArchiveFile_RequestRead:%s\n", fileName);
	if (*fileName == '>')  // filename start with ">rq:" is a section request
	{
		const char* h = fileName;
		//uint32_t filesize = *(uint32_t*)(h + 4);
		//uint16_t fileno = *(uint16_t*)(h + 8);
		uint16_t archive = *(uint16_t*)(h + 10);
		uint32_t offset = *(uint32_t*)(h + 12);
		if (archive == 10) // external section file was assigned to update1.000.tiger 
		{
			offset2hash::iterator it = offset_map.find(offset); // look up hash by file offset
			if (it != offset_map.end())
			{
				ModMap::iterator mod_it = g_ModFileList.find(it->second); // lookup external file name by hash
				if (mod_it != g_ModFileList.end())
				{
					void* req = g_pOrigFS->RequestRead(receiver, mod_it->second.filename.c_str(), 0);
					return req;
				}
			}
		}

	}
	return orgTigerArchiveFileSystem_RequestRead(_this, receiver, fileName, startOffset);
}

// cdc::ResolveReceiver::ReceiveData
int ReceiveData(cdc::ResolveReceiver* _this, void* FileRequest, char* pData, int dataSize, unsigned int param_3)
{
	int num = _this->m_numSections;
	cdc::SectionInfo* secInfoArray;// = _this->m_section;
	cdc::SectionExtraInfo* secExtraInfoArray;// = _this->m_sectionEx;
	int datasize = dataSize;
	int mstate = _this->m_state;
	int* cur_record = (int*)pData;
	int drmVer = cur_record[0];
	if (mstate == 0) // section data processing  started
	{
		int remaining_size = dataSize;
		do {
			// extract section info from data record
			int numSections = cur_record[6];
			int p1_m_dependentDrmLength = cur_record[2];
			int p1_m_includedDrmLength = cur_record[1];

			secInfoArray = (cdc::SectionInfo*)(cur_record + 8);

			int extrainfo_offset = numSections * 0x14 + 0x20 + p1_m_dependentDrmLength + p1_m_includedDrmLength;

			secExtraInfoArray = (cdc::SectionExtraInfo*)((char*)cur_record + extrainfo_offset);
			int record_size = extrainfo_offset + numSections * 0x18;

			for (int i = 0; i < numSections; i++)
			{
				cdc::SectionInfo* si = secInfoArray + i;
				cdc::SectionExtraInfo* sEi = secExtraInfoArray + i;
				uint64_t packedOffset = sEi->packedOffset;

				unsigned long hash = sEi->uniqueId;
				ModMap::iterator it = g_ModFileList.find(hash);
				if (it != g_ModFileList.end())  // check if this section hash is in external mod list
				{
					uint32_t fsize = g_Update1Size;   //update1.000 tiger file size
					// all external section file are assigned to update1.000.tiger, file offset is an unsigned 32bit number.
					// update1 start out at around 500MB. adding too many external sections (mods) will cause offset overflow and crash game.

					uint32_t new_size = (fsize + 0x07ff) & 0xfffff800; // section start on 2k aligned offset
					uint32_t section_offset = new_size + 0x400;    // section offset need to include file header size of the 000.tiger file
					// update section offset and force tiger archive id to 10 (update1.000.tiger)
					sEi->packedOffset = (((uint64_t)section_offset << 0x20) & 0xffffffff00000000) | 0x000a0000; 
					// increase update1 file size
					new_size += it->second.compressed_size;
					// insert extra space after new section to prevent file loader loading sections in batch
					g_Update1Size = (((new_size + 0x7ff) & 0xfffff800) + 0x20800); 
					// update offset to hash map
					offset_map[section_offset] = hash;  

					si->size = it->second.uncompressed_size - (si->packedAsInt >> 8); // substract reclocation records size from uncompress file size
					sEi->compressedSize = it->second.compressed_size;
				}

			}
			remaining_size -= record_size;
			cur_record = (int *)((uint8_t *) cur_record + record_size);
		} while (remaining_size != 0);
	}
	return orgReceiveData(_this, FileRequest, pData, dataSize, param_3);
}

#endif

bool(__cdecl* orgInitPatchArchive)(const char* name);
void(__thiscall* MultiFileSystem_Add)(void* _this, cdc::FileSystem* filesystem, bool unk, bool insertFirst);

bool InitPatchArchive(const char* name)
{
	// init the orginal patch archive, let the game add it to the multi filesystem
	auto ret = orgInitPatchArchive(name);

	// create the hook filesystem and add it front to the multi filesystem
	auto pFS = CreateHookFileSystem(*(cdc::FileSystem**)DISKFS);
	MultiFileSystem_Add(GetFS(), pFS, false, true);

	return ret;
}

#if TRAE || (TR7 && RETAIL_VERSION)
void __cdecl EVENT_DisplayString(char* str, int time)
{
	Hooking::GetInstance().GetMenu()->Log("%s\n", str);
}

void __cdecl EVENT_DisplayStringXY(char* str, int time, int x, int y)
{
	if (!Hooking::GetInstance().GetMenu()->m_drawSettings.drawDebug) return;

	FONT_SetCursor((float)x, (float)y);
	FONT_PrintFormatted(str);
}

void __cdecl EVENT_FontPrint(char* fmt, ...)
{
	if (!Hooking::GetInstance().GetMenu()->m_drawSettings.drawDebug) return;

	va_list vl;
	va_start(vl, fmt);
	char str[1024]; // size same as game buffer
	vsprintf_s(str, fmt, vl);
	va_end(vl);

	FONT_PrintFormatted(str);
}

void __cdecl EVENT_PrintScalarExpression(int val, int time)
{
	if (!Hooking::GetInstance().GetMenu()->m_drawSettings.drawDebug) return;

	FONT_Print("%d", val);
}
#endif

bool(__cdecl* objCheckFamily)(Instance* instance, unsigned __int16 family);

#if TRAE || TR7
void(__cdecl* TRANS_TransToDrawVertexV4f)(DRAWVERTEX* v, cdc::Vector* vec);

void(__cdecl* DRAW_DrawQuads)(int flags, int tpage, DRAWVERTEX* verts, int numquads);

void DrawQuads(cdc::Vector* v0, cdc::Vector* v1)
{
	DRAWVERTEX vertex[4];

	cdc::Vector v3 = *v1;
	cdc::Vector v4 = *v0;
	v3.z += 20.f;
	v4.z += 20.f;

	TRANS_TransToDrawVertexV4f(vertex, v0);
	TRANS_TransToDrawVertexV4f(&vertex[1], v1);
	TRANS_TransToDrawVertexV4f(&vertex[2], &v3);
	TRANS_TransToDrawVertexV4f(&vertex[3], &v4);

	// 4 bytes integer, each r, g, b, a
	// e.g. FF 00 00 FF is 255, 0, 0, 255
	auto color = 4278190335; // red

	vertex[0].color = color;
	vertex[1].color = color;
	vertex[2].color = color;
	vertex[3].color = color;

	DRAW_DrawQuads(2, 0, vertex, 1);
}

void DrawQuads(cdc::Vector* v0, cdc::Vector* v1, cdc::Vector* v2, cdc::Vector* v3, int color)
{
	DRAWVERTEX vertex[4];

	TRANS_TransToDrawVertexV4f(vertex, v0);
	TRANS_TransToDrawVertexV4f(&vertex[1], v1);
	TRANS_TransToDrawVertexV4f(&vertex[2], v2);
	TRANS_TransToDrawVertexV4f(&vertex[3], v3);

	vertex[0].color = color;
	vertex[1].color = color;
	vertex[2].color = color;
	vertex[3].color = color;

	DRAW_DrawQuads(2, 0, vertex, 1);
}

void DrawQuads(int flags, int tpage, DRAWVERTEX* verts, int numquads)
{
	DRAW_DrawQuads(flags, tpage, verts, numquads);
}

std::string FlagToFlags(int flag) noexcept
{
	std::string name;

	for (int i = 0; i < 22; i++)
	{
		if (flag & mudFlags[i].flag)
		{
			name += mudFlags[i].name;
		}
	}

	// somewhat stolen from https://stackoverflow.com/a/37795988/9398242
	if (name.empty())
		name = "NONE";
	else
		name.erase(name.end() - 3, name.end());

	return name;
}

template <typename T>
cdc::Vector GetVertice(unsigned int vertice, Mesh* mesh, cdc::Vector mapposition)
{
	auto vertex = ((T*)mesh->m_vertices)[vertice];

	return cdc::Vector{
		mapposition.x + vertex.x,
		mapposition.y + vertex.y,
		mapposition.z + vertex.z };
}

// TODO move this to font.cpp and add some sort of function to subcribe before Font::Flush
void __cdecl Font__Flush()
{
	auto level = *(Level**)(GAMETRACKER + 8);
	auto drawSettings = Hooking::GetInstance().GetMenu()->m_drawSettings;

	// prints queued file requests
	if (drawSettings.printFileRequests && g_pDiskFS)
	{
		auto queue = g_pDiskFS->m_queue;

		auto y = 15.f;

		for (auto request = queue; request != nullptr; request = request->m_next)
		{
			FONT_SetCursor(15.f, y);
			FONT_Print("%s (%d/%d KB)\n", request->m_pFileName, request->m_bytesRead / 1000, request->m_size / 1000);

			y += FONT_GetHeight();
		}
	}

	// draws collision mesh
	if (drawSettings.drawCollision && level)
	{
		auto terrain = (Terrain*)level->terrain;
		
		for (int group = 0; group < terrain->numTerrainGroups; group++)
		{
			auto terraingroup = terrain->terrainGroups[group];
			auto mesh = terraingroup.mesh;

			if (mesh)
			{
				// loop trough all collision faces
				for (int i = 0; i < mesh->m_numFaces; i++)
				{
					auto face = ((IndexedFace*)mesh->m_faces)[i];

					// get the position of every vertice for this face
					auto x = GetVertice<MeshVertex>(face.i0, mesh, mesh->m_position);
					auto y = GetVertice<MeshVertex>(face.i1, mesh, mesh->m_position);
					auto z = GetVertice<MeshVertex>(face.i2, mesh, mesh->m_position);

					DrawQuads(&x, &y);
					DrawQuads(&y, &z);
					DrawQuads(&z, &x);

					DrawQuads(&x, &y, &z, &x, 167837440);
				}
			}
		}
	}

	// draws signal mesh
	if (level && drawSettings.drawSignals)
	{
		auto terrain = (Terrain*)level->terrain;
		auto signalTerrainGroups = terrain->signalTerrainGroup;

		if (signalTerrainGroups)
		{
			auto mesh = signalTerrainGroups->mesh;

			if (mesh)
			{
				// loop trough all faces
				for (int i = 0; i < mesh->m_numFaces; i++)
				{
					auto face = ((SignalFace*)mesh->m_faces)[i];

					// get the position of every vertice for this face
					auto x = GetVertice<MeshVertex32>(face.i0, mesh, mesh->m_position);
					auto y = GetVertice<MeshVertex32>(face.i1, mesh, mesh->m_position);
					auto z = GetVertice<MeshVertex32>(face.i2, mesh, mesh->m_position);

					DrawQuads(&x, &y, &z, &x, 167772415);
				}
			}
		}
	}

	// draws portals
	if (drawSettings.drawPortals && level)
	{
		auto terrain = *(Terrain*)level->terrain;

		// every portal
		for (int i = 0; i < terrain.numStreamUnitPortals; i++)
		{
			auto portal = terrain.streamUnitPortals[i];

			auto srcVector = cdc::Vector{};

			// TODO vector functions?
			srcVector = cdc::Vector{ (portal.min.x + portal.max.x) / 2, (portal.min.y + portal.max.y) / 2 , (portal.min.z + portal.max.z) / 2 };

			TRANS_RotTransPersVectorf((DWORD)&srcVector, (DWORD)&srcVector);

			// draw text if visible on screen
			if (srcVector.z > 16.f)
			{
				// draw portal id and destination
				FONT_SetCursor(srcVector.x, srcVector.y);
				FONT_Print("portal %d", i);

				srcVector.y += 15.f;
				FONT_SetCursor(srcVector.x, srcVector.y);

				FONT_Print("to %s", portal.tolevelname);

				// draw portal bounds
				auto v2 = cdc::Vector{ portal.min.x, portal.min.y, portal.max.z };
				auto v4 = cdc::Vector{ portal.max.x, portal.max.y, portal.min.z };

				DrawQuads(&portal.min, &v2, &portal.max, &portal.min, 184483840); // rgba(0, 0, 255, 10)
				DrawQuads(&portal.min, &v4, &portal.max, &portal.min, 184483840);
			}
		}
	}

	// draw markup
	if (Hooking::GetInstance().GetMenu()->m_drawSettings.drawMarkup)
	{
#if TR7
		auto markUpManager = *(int*)ADDR(0x1120DC8, 0x1117544);
#elif TRAE
		auto markUpManager = *(int*)0x86CD14;
#endif

		auto box = *(int*)(markUpManager + 0x18);
		while(1)
		{
			auto next = *(int*)(box + 4);

#if TR7
			auto markup = *(int*)(box + 0x20);
#elif TRAE
			auto markup = *(int*)(box + 0xC);
#endif
			if (markup)
			{
#if TR7
				auto polyline = *(int*)(markup + 0x2C);
#elif TRAE
				auto polyline = *(int*)(markup + 0x48);
#endif

				auto srcVector = cdc::Vector{};
#if TR7
				srcVector = *(cdc::Vector*)(markup + 0x14);
#elif TRAE
				srcVector = *(cdc::Vector*)(markup + 0x30);
#endif
				TRANS_RotTransPersVectorf((DWORD)&srcVector, (DWORD)&srcVector);

				if (srcVector.z > 16.f)
				{
#if TRAE
					auto flags = *(int*)(box + 0x28);
#elif TR7
					auto flags = *(int*)(box + 0x24);
#endif

					// display the markup flags
					FONT_SetCursor(srcVector.x, srcVector.y);
					FONT_Print("%s", FlagToFlags(flags).c_str());
				}

				if (polyline)
				{
					// draw the polyline
					auto numPoints = *(int*)(polyline);

					auto x = (cdc::Vector*)(polyline + 0x10);
					for (int j = 0; j < numPoints; j++)
					{
						auto y = (cdc::Vector*)(polyline + 16 * (j + 1));

						// no known drawline for TRAE without writing lot of manual code
						// so write quads
						// if you want to try, s_pLinePool is 0x7545E0 in TRAE
						DrawQuads(x, y);

						x = y;
					}
				}
			}

			if (!next)
				break;

			box = next;
		}
	}
	
	auto settings = Hooking::GetInstance().GetMenu()->m_drawSettings;

	// draw instances
	if (settings.draw || settings.drawPath)
	{
		// loop trough all instances
		for (auto instance = *(Instance**)INSTANCELIST; instance != nullptr; instance = instance->next)
		{
			auto object = instance->object;
			auto data = *(DWORD*)((DWORD)instance + 448);
			auto extraData = *(DWORD*)((DWORD)instance + 572);

			if (settings.drawPath)
			{
				if (extraData && *(unsigned __int16*)(data + 2) == 56048)
				{
#if TR7
					auto routing = extraData + 0x1060;
#elif TRAE
					auto routing = extraData + 4432;
#endif
					auto path = routing + 0x90;
					auto length = *(int*)(path + 0x1A8);

					auto x = (cdc::Vector*)(path);
					for (int j = 0; j < length - 1; j++)
					{
						auto y = (cdc::Vector*)(path + 16 * (j + 1));

						DrawQuads(x, y);

						x = y;
					}
				}
			}

			// TODO filter only pickups
			auto show = [](DrawSettings settings, Instance* instance, DWORD data)
			{
				if (strlen(settings.filterName) > 0)
				{
					return strstr(instance->object->name, settings.filterName) != 0;
				}

				if (!settings.filter) return true;

				// if selected 'draw enemy health' and instance is an enemy continue
				if (settings.drawHealth && data && *(unsigned __int16*)(data + 2) == 56048) return true;

				return objCheckFamily(instance, 35) /* keys, healthpacks stuff */ || objCheckFamily(instance, 39) /* ammo */;
			};

			auto srcVector = cdc::Vector{};
			srcVector = instance->position;
			TRANS_RotTransPersVectorf((DWORD)&srcVector, (DWORD)&srcVector);

			if (settings.draw && show(settings, instance, data) && srcVector.z > 16.f /* only draw when on screen */)
			{
				FONT_SetCursor(srcVector.x, srcVector.y);
				FONT_Print("%s", object->name);

				if (settings.drawHealth && extraData && data && *(unsigned __int16*)(data + 2) == 56048)
				{
#if TRAE
					auto health = *(float*)(extraData + 5280);
#elif TR7
					auto health = *(float*)(extraData + 5040);
#endif
					srcVector.y += 15.f;
					FONT_SetCursor(srcVector.x, srcVector.y);
					FONT_Print("%8.2f", health);
				}

				if (settings.drawIntro)
				{
					srcVector.y += 15.f;
					FONT_SetCursor(srcVector.x, srcVector.y);
					FONT_Print("Intro %d", instance->introUniqueID);
				}

				if (settings.drawAddress)
				{
					srcVector.y += 15.f;
					FONT_SetCursor(srcVector.x, srcVector.y);
					FONT_Print("%p", instance);
				}

				if (settings.drawFamily && data)
				{
					srcVector.y += 15.f;
					FONT_SetCursor(srcVector.x, srcVector.y);
					FONT_Print("Family %d", *(unsigned __int16*)(data + 2));
				}

				if (settings.drawAnim && instance->object->numAnims > 0)
				{
					auto anim = G2EmulationInstanceQueryAnimation(instance, 0);

					srcVector.y += 15.f;
					FONT_SetCursor(srcVector.x, srcVector.y);
					FONT_Print("Anim %d (%X)", anim, instance->object->animList[anim].animationID);
				}
			}
		}
	}

	org_Font__Flush();
}
#endif

void Hooking::GotDevice()
{
	this->m_menu = std::make_unique<Menu>(pDevice, pHwnd);
	this->m_menu->SetVisibility(!m_config.hide_menu_on_start);

	// hook game's d3d9 present function and wndproc function
#if TRAE
	MH_CreateHook(reinterpret_cast<void*>(0x61BB80), hooked_PCRenderContext_Present, reinterpret_cast<void**>(&original_PCRenderContext_Present));

	MH_CreateHook(reinterpret_cast<void*>(0x4040B0), hooked_RegularWndProc, reinterpret_cast<void**>(&original_RegularWndProc));
#elif TR8
	MH_CreateHook(reinterpret_cast<void*>(0x519360), hooked_PCRenderContext_Present, reinterpret_cast<void**>(&original_PCRenderContext_Present));

	MH_CreateHook(reinterpret_cast<void*>(0x478BC0), hooked_RegularWndProc, reinterpret_cast<void**>(&original_RegularWndProc));
#elif TR7
	auto present = FindPattern((PBYTE)"\x8B\x41\x14\x85\xC0\x74\x19\x8B\x54\x24\x0C", "xxxxxxxxxxx");
	MH_CreateHook(reinterpret_cast<void*>(present), hooked_PCRenderContext_Present, reinterpret_cast<void**>(&original_PCRenderContext_Present));

	auto wndProc = FindPattern((PBYTE)"\x83\xEC\x68\x55", "xxxx");
	MH_CreateHook(reinterpret_cast<void*>(wndProc), hooked_RegularWndProc, reinterpret_cast<void**>(&original_RegularWndProc));
#endif

	// hook SetCursorPos to prevent the game from resetting the cursor position
	MH_CreateHookApi(L"user32", "SetCursorPos", hooked_SetCursorPos, reinterpret_cast<void**>(&original_SetCursorPos));

#if TRAE
	MH_CreateHook((void*)0x00617F50, PCDeviceManager__ReleaseDevice, (void**)&orginal_PCDeviceManager__ReleaseDevice);
	MH_CreateHook((void*)0x00617BE0, PCDeviceManager__CreateDevice, (void**)&original__PCDeviceManager__CreateDevice);
#elif TR8
	MH_CreateHook((void*)0x005223F0, PCDeviceManager__ReleaseDevice, (void**)&orginal_PCDeviceManager__ReleaseDevice);
	MH_CreateHook((void*)0x00522580, PCDeviceManager__CreateDevice, (void**)&original__PCDeviceManager__CreateDevice);
#elif TR7
	MH_CreateHook((void*)ADDR(0xECCC20, 0xEC6550), PCDeviceManager__ReleaseDevice, (void**)&orginal_PCDeviceManager__ReleaseDevice);
	MH_CreateHook((void*)ADDR(0xECC8F0, 0xEC62A0), PCDeviceManager__CreateDevice, (void**)&original__PCDeviceManager__CreateDevice);
#endif

#if TRAE
	// patch debug print nullsubs to our functions
	*(DWORD*)(GLOBALDATA + 528) = (DWORD)EVENT_DisplayString;
	*(DWORD*)(GLOBALDATA + 304) = (DWORD)EVENT_DisplayString;

	// draw debug
	*(DWORD*)(GLOBALDATA + 1400) = (DWORD)EVENT_DisplayStringXY;
	*(DWORD*)(GLOBALDATA + 464) = (DWORD)EVENT_FontPrint;
	*(DWORD*)(GLOBALDATA + 1292) = (DWORD)EVENT_PrintScalarExpression;
#elif (TR7 && RETAIL_VERSION)
	*(DWORD*)(GLOBALDATA + 504) = (DWORD)EVENT_DisplayString;
	*(DWORD*)(GLOBALDATA + 304) = (DWORD)EVENT_DisplayString;

	*(DWORD*)(GLOBALDATA + 1336) = (DWORD)EVENT_DisplayStringXY;
	*(DWORD*)(GLOBALDATA + 444) = (DWORD)EVENT_FontPrint;
	*(DWORD*)(GLOBALDATA + 1232) = (DWORD)EVENT_PrintScalarExpression;
#endif

#if TRAE
	objCheckFamily = reinterpret_cast<bool(__cdecl*)(Instance* instance, unsigned __int16 family)>(0x534660);

	MH_CreateHook((void*)0x00434C40, Font__Flush, (void**)&org_Font__Flush);

	TRANS_RotTransPersVectorf = reinterpret_cast<float*(__cdecl*)(DWORD, DWORD)>(0x00402B50);

	TRANS_TransToDrawVertexV4f = reinterpret_cast<void(__cdecl*)(DRAWVERTEX* v, cdc::Vector * vec)>(0x00402F20);

	DRAW_DrawQuads = reinterpret_cast<void(__cdecl*)(int flags, int tpage, DRAWVERTEX * verts, int numquads)>(0x00406D70);
#elif TR7
	MH_CreateHook((void*)ADDR(0x435050, 0x432570), Font__Flush, (void**)&org_Font__Flush);

	TRANS_RotTransPersVectorf = reinterpret_cast<float* (__cdecl*)(DWORD, DWORD)>(ADDR(0x402D00, 0x402B20));

	TRANS_TransToDrawVertexV4f = reinterpret_cast<void(__cdecl*)(DRAWVERTEX * v, cdc::Vector * vec)>(ADDR(0x4030D0, 0x402EF0));

	DRAW_DrawQuads = reinterpret_cast<void(__cdecl*)(int flags, int tpage, DRAWVERTEX * verts, int numquads)>(ADDR(0x406720, 0x406240));

	objCheckFamily = reinterpret_cast<bool(__cdecl*)(Instance* instance, unsigned __int16 family)>(ADDR(0x5369C0, 0x531B10));

	#if !RETAIL_VERSION
		// nop out useless F3 mouse toggle to be replaced by our F3
		NOP((void*)0x405559, 5);
	#endif
#endif

#if TR8
	MH_CreateHook((void*)0x4A3280, ScriptPrintInt, nullptr);
	MH_CreateHook((void*)0x4A32C0, ScriptPrintFloat, nullptr);
	MH_CreateHook((void*)0x795DB0, NsCoreBase_PrintString, nullptr);

	MH_CreateHook((void*)0x4A3200, ScriptLogInt, nullptr);
	MH_CreateHook((void*)0x4A3240, ScriptLogFloat, nullptr);
	MH_CreateHook((void*)0x795E30, NsCoreBase_PrintString, nullptr);
#endif

	MH_EnableHook(MH_ALL_HOOKS);
}

int hooked_Direct3DInit()
{
	// call orginal game function to init d3direct device
	auto val = original_Direct3DInit();

	// get the d3d9 device and hwnd
#if TRAE
	pHwnd = *reinterpret_cast<HWND*>(0x6926C8);
#elif TR8
	pHwnd = *reinterpret_cast<HWND*>(0x9EEDE8);
#elif TR7
	auto pFound = FindPattern((PBYTE)"\x8B\x0D\x00\x00\x00\x00\x56\x51\xE8\x00\x00\x00\x00\xE8", "xx????xxx????x");

	pHwnd = **(HWND**)(pFound + 2);
#endif

	// (IDirect3DDevice*)devicemanager->d3device
#if TRAE
	auto address = *reinterpret_cast<DWORD*>(0xA6669C);
#elif TR8
	auto address = *reinterpret_cast<DWORD*>(0xAD75E4);
#elif TR7
	pFound = FindPattern((PBYTE)"\x8B\x0D\x00\x00\x00\x00\x68\x00\x00\x00\x00\x50\xE8", "xx????x????xx");
	auto address = **(int**)(pFound + 2);
#endif
#if !defined(ROTTR)
	auto device = *reinterpret_cast<DWORD*>(address + 0x20);
	pDevice = reinterpret_cast<IDirect3DDevice9*>(device);

	// if direct3d initialisation fails using the device will crash the game instead of showing game dx9 error
	if (pDevice != nullptr)
	{
		Hooking::GetInstance().GotDevice();
	}
#endif
	return val;
}

#if TRAE || TR7
void __fastcall hooked_PCRenderContext_Present(DWORD* _this, void* _, int a2, int a3, int a4)
{
	if (pDevice)
	{
		Hooking::GetInstance().GetMenu()->OnPresent();
	}

	// call orginal game present function to draw on the screen
	original_PCRenderContext_Present(_this, a2, a3, a4);
}
#elif TR8
void __fastcall hooked_PCRenderContext_Present(DWORD* _this, void* _, int a2)
{

	// call orginal game present function to draw on the screen
	original_PCRenderContext_Present(_this, a2);
	
	Hooking::GetInstance().GetMenu()->OnPresent();
}
#endif

LRESULT hooked_RegularWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	auto menu = Hooking::GetInstance().GetMenu();

	if (msg == WM_KEYUP && wparam == VK_F8)
	{
		auto focus = menu->IsFocus();

		// set menu focus
		menu->SetFocus(!focus);
		menu->SetVisibility(!focus);
	}

#if TRAE
	if (msg == WM_CLOSE
		&& Hooking::GetInstance().GetConfig().remove_quit_message)
	{
		PostQuitMessage(0);
	}
#endif

	// pass input to menu
	menu->Process(hwnd, msg, wparam, lparam);

	// pass input to orginal game wndproc
	return original_RegularWndProc(hwnd, msg, wparam, lparam);
}

BOOL WINAPI hooked_SetCursorPos(int x, int y)
{
	// prevent game from reseting cursor position
	if (Hooking::GetInstance().GetMenu()->IsFocus())
	{
		return 1;
	}

	return original_SetCursorPos(x, y);
}

void NOP(void* address, int num)
{
	DWORD lpflOldProtect, _;
	VirtualProtect(address, num, PAGE_EXECUTE_READWRITE, &lpflOldProtect);
	memset(address, 0x90, num);
	VirtualProtect(address, num, lpflOldProtect, &_);
}