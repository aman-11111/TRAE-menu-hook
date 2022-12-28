#pragma once

#include <memory>
#include <map>
#include <string>
#include "vendor/MinHook.h"
#include "Game.hpp"
#include "Menu.hpp"

#include "game/filesystem.hpp"

namespace mh
{
	struct Config
	{
		bool remove_legal_screen = true;
		bool remove_quit_message = true;
		bool hide_menu_on_start = false;
	};
}

class Hooking
{
public:
	static Hooking& GetInstance()
	{
		static Hooking instance;
		return instance;
	}

	Hooking();
	~Hooking();

	Hooking(Hooking const&) = delete;
	void operator=(Hooking const&) = delete;

	void GotDevice();
	std::shared_ptr<Menu>& GetMenu() noexcept;

	mh::Config& GetConfig();
	void LoadConfig();

private:
	std::shared_ptr<Menu> m_menu;

	mh::Config m_config;
};

static int(*original_Direct3DInit)();
int hooked_Direct3DInit();

#if TRAE || TR7
static void(__thiscall* original_PCRenderContext_Present)(DWORD*, int, int, int);
void __fastcall hooked_PCRenderContext_Present(DWORD* _this, void* _, int a2, int a3, int a4);
#elif TR8
static void(__thiscall* original_PCRenderContext_Present)(DWORD*, int);
void __fastcall hooked_PCRenderContext_Present(DWORD* _this, void* _, int a2);
#endif

static LRESULT(__stdcall* original_RegularWndProc)(HWND, UINT, WPARAM, LPARAM);
static LRESULT hooked_RegularWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static BOOL(WINAPI* original_SetCursorPos)(int, int);
static BOOL WINAPI hooked_SetCursorPos(int x, int y);

void NOP(void* address, int num);

uint8_t __declspec(noinline)* FindPattern(BYTE* bMask, char* szMask);
uint8_t __declspec(noinline)* GetAddress(uint8_t* ptr, uint8_t offset, uint8_t instr_size);

void DrawQuads(int flags, int tpage, DRAWVERTEX* verts, int numquads);

extern int(__cdecl* G2EmulationInstanceQueryAnimation)(Instance* instance, int section);

bool InitPatchArchive(const char* name);

extern bool(__cdecl* orgInitPatchArchive)(const char* name);
extern void(__thiscall* MultiFileSystem_Add)(void* _this, cdc::FileSystem* filesystem, bool unk, bool insertFirst);

#if ROTTR
class ModInfo {
public:
	ModInfo()
	{
		filename = "";
		compressed_size = 0;
		uncompressed_size = 0;
	}
	ModInfo(const char* f, unsigned long c, unsigned long u)
	{
		filename = f;
		compressed_size = c;
		uncompressed_size = u;
	}
	std::string filename;
	unsigned long compressed_size, uncompressed_size;
};

extern cdc::FileSystem* g_pOrigFS;

typedef std::map<uint32_t, ModInfo> ModMap;
extern  ModMap g_ModFileList;

extern void ReadModFileList();

void DLCSystem_Create(void);
extern void(__cdecl* orgDLCSystem_Create)(void);

void* TigerArchiveFileSystem_RequestRead(void* _this, void* receiver, const char* fileName, unsigned int startOffset);
extern void* (__cdecl* orgTigerArchiveFileSystem_RequestRead)(void* _this, void* receiver, const char* fileName, unsigned int startOffset);

int ReceiveData(cdc::ResolveReceiver* _this, void* FileRequest, char* param_1, int param_2, unsigned int param_3);
extern int(__cdecl* orgReceiveData)(cdc::ResolveReceiver* _this, void* FileRequest, char* param_1, int param_2, unsigned int param_3);

#endif