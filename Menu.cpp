#include "Menu.hpp"
#include "Game.hpp"
#include "Hooking.hpp"

#include "game/event.hpp"
#include "game/stream/stream.hpp"
#include "game/obtable.hpp"

static bool shouldInstance = true;
static bool shouldReloc = true;

Instance*(__cdecl* INSTANCE_NewInstance)();

Instance* newinstance()
{
    if (shouldInstance)
        return INSTANCE_NewInstance();

    return 0;
}

char IsPs2()
{
    // somewhat the zoom code for binoculars is inside an 'if IsPs2()' in TRAE so we have to hook that function
    return Game::m_binoculars;
}

bool switchPlayerNextFrame = false;

void(__cdecl* origSTREAM_FinishLoad)(StreamUnit* unit);
void STREAM_FinishLoad(StreamUnit* unit)
{
    if (!shouldReloc)
    {
        Level* level = unit->level;
        level->reloc = 0; // set reloc module ptr to 0
    }

    origSTREAM_FinishLoad(unit);
}

int(__cdecl* INSTANCE_ReallyRemoveInstance)(Instance* instance, int a2, char a3);
void(__cdecl* INSTANCE_SetModel)(Instance* instance, int model);

void(__cdecl* G2EmulationInstanceSetAnimation)(Instance* instance, int section, int anim, int frame, int frames);
void(__cdecl* G2EmulationInstanceSetMode)(Instance* instance, int section, int mode);
int(__cdecl* G2EmulationInstanceQueryAnimation)(Instance* a1, int section);

void(__cdecl* INSTANCE_HideUnhideDrawGroup)(Instance*, int, int);

void(__thiscall* origCinematicHandlerImpl_NextFrame)(int _this);
void __fastcall CinematicHandlerImpl_NextFrame(int _this, int)
{
    auto streamFlags = (int*)0x8383F4;
    if (*streamFlags & 0x1000)
    {
        // dont progress cine
        return;
    }

    origCinematicHandlerImpl_NextFrame(_this);
}

#if TR8
void(__cdecl* CAMERA_SetMode)(int mode);
int(__thiscall* _setToGameplayCamera)(int _this);
#endif

int(__cdecl* RELOC_GetProcAddress)(int reloc, const char* symbol);

void(__cdecl* LOAD_UnitFileName)(char* name, char* unit, char* extension);

Menu::Menu(LPDIRECT3DDEVICE9 pd3dDevice, HWND hwnd)
{
	m_pd3dDevice = pd3dDevice;
	m_hwnd = hwnd;

	OnLayoutChange();
	LoadSettings();

	ImGui::CreateContext();

	ImGui_ImplWin32_Init(m_hwnd);
	ImGui_ImplDX9_Init(m_pd3dDevice);


#if TRAE
    MH_CreateHook((void*)0x00457580, newinstance, (void**)&INSTANCE_NewInstance);

    MH_CreateHook((void*)0x004E6EC0, IsPs2, nullptr);
    MH_CreateHook((void*)0x005DB680, STREAM_FinishLoad, (void**)&origSTREAM_FinishLoad);

    MH_CreateHook((void*)0x00424FE0, CinematicHandlerImpl_NextFrame, (void**)&origCinematicHandlerImpl_NextFrame);
#endif

#if TRAE
    INSTANCE_ReallyRemoveInstance = reinterpret_cast<int(__cdecl*)(Instance*, int, char)>(0x0045A3A0);
    INSTANCE_SetModel = reinterpret_cast<void(__cdecl*)(Instance * instance, int model)>(0x00458A90);

    G2EmulationInstanceSetAnimation = reinterpret_cast<void(__cdecl*)(Instance*, int, int, int, int)>(0x004DE690);
    G2EmulationInstanceSetMode = reinterpret_cast<void(__cdecl*)(Instance*, int, int)>(0x004DE7F0);
    G2EmulationInstanceQueryAnimation = reinterpret_cast<int(__cdecl*)(Instance*, int)>(0x004DE8B0);

    INSTANCE_HideUnhideDrawGroup = reinterpret_cast<void(__cdecl*)(Instance*, int, int)>(0x004319B0);

    RELOC_GetProcAddress = reinterpret_cast<int(__cdecl*)(int, const char*)>(0x4680C0);
    LOAD_UnitFileName = reinterpret_cast<void(__cdecl*)(char*, char*, char*)>(0x45F650);
#elif TR7
    MH_CreateHook((void*)ADDR(0x457730, 0x454A00), newinstance, (void**)&INSTANCE_NewInstance);
    MH_CreateHook((void*)ADDR(0x5DB800, 0x5D5640), STREAM_FinishLoad, (void**)&origSTREAM_FinishLoad);

    INSTANCE_ReallyRemoveInstance = reinterpret_cast<int(__cdecl*)(Instance*, int, char)>(ADDR(0x45A330, 0x4575B0));
    INSTANCE_SetModel = reinterpret_cast<void(__cdecl*)(Instance * instance, int model)>(ADDR(0x458C80, 0x455F40));

    G2EmulationInstanceSetAnimation = reinterpret_cast<void(__cdecl*)(Instance*, int, int, int, int)>(ADDR(0x4E1F00, 0x4DEC30));
    G2EmulationInstanceSetMode = reinterpret_cast<void(__cdecl*)(Instance*, int, int)>(ADDR(0x4E2060, 0x4DED90));
    G2EmulationInstanceQueryAnimation = reinterpret_cast<int(__cdecl*)(Instance*, int)>(ADDR(0x4E2120, 0x4DEE50));

    INSTANCE_HideUnhideDrawGroup = reinterpret_cast<void(__cdecl*)(Instance*, int, int)>(ADDR(0x458FB0, 0x456230));

    RELOC_GetProcAddress = reinterpret_cast<int(__cdecl*)(int, const char*)>(ADDR(0x467570, 0x464550));
    LOAD_UnitFileName = reinterpret_cast<void(__cdecl*)(char*, char*, char*)>(ADDR(0x45F4D0, 0x45C730));
#elif TR8
    INSTANCE_ReallyRemoveInstance = reinterpret_cast<int(__cdecl*)(Instance*, int, char)>(0x005BC4E0);
    INSTANCE_SetModel = reinterpret_cast<void(__cdecl*)(Instance * instance, int model)>(0x005B9170);

    G2EmulationInstanceSetAnimation = reinterpret_cast<void(__cdecl*)(Instance*, int, int, int, int)>(0x005B1EA0);
    G2EmulationInstanceSetMode = reinterpret_cast<void(__cdecl*)(Instance*, int, int)>(0x005B1F50);

    CAMERA_SetMode = reinterpret_cast<void(__cdecl*)(int mode)>(0x005F39F0);
    _setToGameplayCamera = reinterpret_cast<int(__thiscall*)(int _this)>(0x5EDF50);

    LOAD_UnitFileName = reinterpret_cast<void(__cdecl*)(char*, char*, char*)>(0x477970);
#endif
}

void Menu::OnDeviceReleased()
{
    m_pd3dDevice->Release();
}

void Menu::SetDevice(LPDIRECT3DDEVICE9 pd3dDevice)
{
    m_pd3dDevice = pd3dDevice;
    ImGui_ImplDX9_Init(m_pd3dDevice);
}

void Menu::OnPresent()
{
	ImGui::GetIO().MouseDrawCursor = m_focus;

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	// draw menu
    if (m_visible)
    {
        this->Draw();
    }

	ImGui::EndFrame();

	ImGui::Render();
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void Menu::Process(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (msg == WM_KEYUP && wparam == VK_F2)
    {
        ToggleFlight(!m_flight);
    }

    if (msg == WM_KEYUP && wparam == VK_F9)
    {
        Game::SwitchPlayerCharacter();
    }

    if (msg == WM_KEYUP && wparam == VK_F11)
    {
#if TRAE || TR7
        Game::InstancePost(*(Instance**)PLAYERINSTANCE, 262167, 3);
#elif TR8
        // instance message types were changed in Underworld
        Game::InstancePost(*(Instance**)PLAYERINSTANCE, 4, 3);
#endif
    }

    if (msg == WM_KEYUP && wparam == VK_F7)
    {
        m_visible = !m_visible;

        // if menu is focussed and being hidden also stop focus
        if (m_focus && !m_visible)
        {
            SetFocus(false);
        }
    }

    if (msg == WM_KEYUP && wparam == VK_F5)
    {
#if TR8
        auto gPlayerControl_Enabled = (bool*)0x8AB4E6;
#elif TRAE
        auto gPlayerControl_Enabled = (bool*)0x666C34;
#elif TR7
        auto gPlayerControl_Enabled = (bool*)ADDR(0xF1E40C, 0xF15AB4);
#elif ROTTR
        auto gPlayerControl_Enabled = (bool*)0x8AB4E6; // dummy address
#endif

        *gPlayerControl_Enabled = !*gPlayerControl_Enabled;
    }

    if (msg == WM_KEYUP && wparam == VK_F3 && shift)
    {
#if TRAE || TR7
        auto debugTimeMult = (float*)(GAMETRACKER + 0x13C);
#elif TR8
        auto debugTimeMult = (float*)(GAMETRACKER + 0x148);
#elif ROTTR
        auto debugTimeMult = (float*)(GAMETRACKER + 0x148);
#endif

        if (*debugTimeMult < 0.3f)
        {
            *debugTimeMult = 1.0f;
        }
        else if (*debugTimeMult < 0.5f)
        {
            *debugTimeMult = 0.2f;
        }
        else
        {
            *debugTimeMult = 0.4f;
        }
    }

    if (msg == WM_KEYUP && wparam == VK_F4)
    {
        if (m_freeCamMode == FreeCameraMode::disabled)
        {
            // enable free camera
            m_freeCamMode = FreeCameraMode::enabled;

            // switch to camera
#if TR8
            CAMERA_SetMode(11);

            auto camera = *(int*)0xE80534;
            *(cdc::Vector*)(camera + 0x40) = (*(Instance**)PLAYERINSTANCE)->position;
#elif TRAE || (TR7 && RETAIL_VERSION)
            *(__int16*)CAMERAMODE = 7;
#endif
        }
        else if (m_freeCamMode == FreeCameraMode::enabled)
        {
            // disable free camera but keep camera active
            m_freeCamMode = FreeCameraMode::nocontrol;
        }
        else if (m_freeCamMode == FreeCameraMode::nocontrol)
        {
            m_freeCamMode = FreeCameraMode::disabled;

            // switch back to gameplay camera
#if TR8
            _setToGameplayCamera(0xE804F0 /* AVCameraManager */);
#elif TRAE || (TR7 && RETAIL_VERSION)
            *(__int16*)CAMERAMODE = 2;
#endif
        }
    }

    // pause the game with F3
    if (msg == WM_KEYUP && wparam == VK_F3 && !shift)
    {
#if TRAE || TR7
        auto streamFlags = (int*)(GAMETRACKER + 0xC4);
#elif TR8
        auto streamFlags = (int*)(GAMETRACKER + 0x30);
#elif ROTTR
        auto streamFlags = (int*)(GAMETRACKER + 0x30);
#endif
        if (*streamFlags & 0x1000)
        {
            *streamFlags &= 0xFFFFEFFF;
        }
        else
        {
            *streamFlags |= 0x1000u;
        }
    }

#if TRAE
    if (msg == WM_KEYUP && wparam == VK_DELETE)
    {
        Game::ToggleBinoculars();
    }
#endif

    if (switchPlayerNextFrame)
    {
        switchPlayerNextFrame = false;

        Game::SwitchPlayerCharacter();
    }

#if TRAE || TR7
    if (msg == WM_KEYUP && wparam == VK_INSERT)
    {
        *(int*)(GAMETRACKER + 0x1C) ^= 0x8000; // hide hud
    }
#endif

    if (msg == WM_INPUTLANGCHANGE)
    {
        OnLayoutChange();
    }

    if (m_flight)
    {
        ProcessFlight(msg, wparam);
    }

	if (m_focus)
	{
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
	}
}

void Menu::ToggleFlight(bool flight)
{
    m_flight = flight;

#if TRAE || TR7
    auto ptr = reinterpret_cast<int*>(GAMETRACKER + 227);
#elif TR8
    auto ptr = reinterpret_cast<int*>(0xE7F143);
#elif ROTTR
    auto ptr = reinterpret_cast<int*>(0xE7F143); 
#endif

    if (m_flight)
    {
        //*ptr |= 256;
        *ptr |= 256;
    }
    else
    {
        //*ptr ^= 256;
        *ptr = 0;
    }

#if TRAE || TR7
    Game::InstancePost(*reinterpret_cast<Instance**>(PLAYERINSTANCE), 1048592, m_flight);
#else
    Game::InstancePost(*reinterpret_cast<Instance**>(PLAYERINSTANCE), 12, m_flight);
#endif
}

void Menu::ProcessFlight(UINT msg, WPARAM wparam)
{
    auto base = *reinterpret_cast<DWORD*>(PLAYERINSTANCE);

#if TRAE || TR7
    auto z = reinterpret_cast<float*>(base + 24);
#elif TR8
    auto z = reinterpret_cast<float*>(base + 40);
#elif ROTTR
    auto z = reinterpret_cast<float*>(base + 40);
#endif

    auto upKey = m_isAzertyLayout ? 0x41 /* A */ : 0x51 /* Q */;
    auto downKey = m_isAzertyLayout ? 0x57 /* W */ : 0x5A /* Z */;

    if (msg == WM_KEYDOWN && wparam == upKey)
    {
        *z += m_flightSpeed;
    }

    if (msg == WM_KEYDOWN && wparam == downKey)
    {
        *z -= m_flightSpeed;
    }
}

// used for PushOkDialog so it pops the dialog away after pressing OK
void __cdecl DialogFn()
{
    Game::PopScreen();
}

void Menu::Draw()
{
    static bool show_instance_viewer = false;
    static bool show_event_debug_viewer = false;

    if (show_instance_viewer) DrawInstanceViewer(&show_instance_viewer);

#if TRAE || TR7 // event system doesnt exist in Underworld
    if (show_event_debug_viewer) DrawEventDebugViewer(&show_event_debug_viewer);
#endif

    ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("Instance viewer", NULL, &show_instance_viewer);

#if TRAE || TR7
            ImGui::MenuItem("Event debug", NULL, &show_event_debug_viewer);
#endif
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("GitHub"))
            {
                ShellExecuteA(0, 0, "https://github.com/TheIndra55/TRAE-menu-hook", 0, 0, SW_SHOW);
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::Text("Unit = %s, Flight = %s", (char*)GAMETRACKER_BASE_AREA, m_flight ? "true" : "false");
    ImGui::SliderFloat("Z speed", &m_flightSpeed, 0, 500);

    ImGui::Checkbox("No deathfade", &m_drawSettings.noRespawn);
#if TRAE || TR7
    if (ImGui::Checkbox("No cinematic bars", &m_drawSettings.noMovieBars))
    {
        SaveSettings();
    }

    if (ImGui::Checkbox("No motion blur", &m_drawSettings.noMotionBlur))
    {
        SaveSettings();
    }
#endif

#if TRAE
    if (ImGui::Button("Fill 'er Up"))
    {
        // pointers everywhere!
        auto missingHealth = *(float*)(*(DWORD*)((*(DWORD*)PLAYERINSTANCE) + 448) + 164);

        Game::IncreaseHealth(missingHealth);
        Game::TriggerUiFadeGroup(1);
        // TODO ammo
    }
#endif

#if TRAE || TR7
    ImGui::Checkbox("Should instance?", &shouldInstance);
    ImGui::Checkbox("Load unit script", &shouldReloc);

    if (ImGui::Checkbox("Enable debug draw", &m_drawSettings.drawDebug))
    {
        *(int*)(GAMETRACKER + 0x18) /*debugFlags2*/ |= 0x100;
    }

    ImGui::Checkbox("Show load queue", &m_drawSettings.printFileRequests);
#endif

#if TRAE
    ImGui::Checkbox("Enable debug keypad", (bool*)0x7C8A3C);
    ImGui::Checkbox("Wireframe", (bool*)0x7C7CD4);
#elif TR7
    ImGui::Checkbox("Enable debug keypad", (bool*)ADDR(0x107F90C, 0x107696C));
    ImGui::Checkbox("Wireframe", (bool*)ADDR(0x107E580, 0x1075BD4));
#endif

#if TRAE || TR7
    ImGui::Checkbox("Draw instances", &m_drawSettings.draw);
    ImGui::Checkbox("Draw markup", &m_drawSettings.drawMarkup);
    ImGui::Checkbox("Draw enemy path", &m_drawSettings.drawPath);
    ImGui::Checkbox("Draw collision mesh", &m_drawSettings.drawCollision);
    ImGui::Checkbox("Draw portals", &m_drawSettings.drawPortals);
    ImGui::Checkbox("Draw signals", &m_drawSettings.drawSignals);

    if (ImGui::CollapsingHeader("Draw settings"))
    {
        ImGui::InputText("filter", m_drawSettings.filterName, 100);

        ImGui::Checkbox("Filter family", &m_drawSettings.filter);
        ImGui::Checkbox("Draw intro id", &m_drawSettings.drawIntro);
        ImGui::Checkbox("Draw animation", &m_drawSettings.drawAnim);
        ImGui::Checkbox("Draw pointer address", &m_drawSettings.drawAddress);
        ImGui::Checkbox("Draw 'family'", &m_drawSettings.drawFamily);
        ImGui::Checkbox("Draw enemy health", &m_drawSettings.drawHealth);
    }
#endif

    // unit switch
    static char* unit = nullptr;
    if (ImGui::BeginCombo("##Unit", unit))
    {
        auto unitList = gUnitList;

        for (int i = 0; i < unitList->numUnits; i++)
        {
            auto x = &unitList->units[i];

            if (strlen(x->name) > 0)
            {
                bool selected = unit == x->name;

                if (ImGui::Selectable(x->name, selected))
                    unit = x->name;

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("Load unit"))
    {
        // check if any file exists, this does not check if file is actually a unit
        char drmName[256];
        LOAD_UnitFileName(drmName, unit, "drm");

        if (unit && GetFS()->FileExists(drmName))
        {
            // change current unit
            strcpy_s((char*)GAMETRACKER_BASE_AREA, MAX_UNIT_LEN, unit);
            Game::ResetGame(4);
        }
    }

#if TRAE
    ImGui::Text("screen: %d", Game::GetTopScreenID());

    static char screenId[8] = "";
    ImGui::InputText("screen Id", screenId, 8);
    if (ImGui::Button("Push screen"))
    {
        int screen = std::atoi(screenId);

        Game::PushScreen(screen, 0);
    }
#endif

    if (ImGui::Button("Clear")) {
        this->logBuffer.clear();
    }

#if TR8
    if (ImGui::Button("Unhide all instances"))
    {
        // loop trough all instances
        for (auto instance = *(Instance**)INSTANCELIST; instance != nullptr; instance = instance->next)
        {
            // send post message to toggle hide
            Game::InstancePost(instance, 7, 0);
        }
    }
#endif

    if (ImGui::Button("List instances"))
    {
        // loop trough all instances and log to console
        for (auto instance = *(Instance**)INSTANCELIST; instance != nullptr; instance = instance->next)
        {
            Log("%s - %d\n", instance->object->name, instance->introUniqueID);
        }

        // copy console buffer to clipboard
        ImGui::SetClipboardText(this->logBuffer.begin());
    }

    auto player = *reinterpret_cast<Instance**>(PLAYERINSTANCE);
#if TRAE
    if (ImGui::Button("Trigger All Fade Groups"))
    {
        for (int i = 0; i <= 28; i++)
        {
            Game::TriggerUiFadeGroup(i);
        }

        Game::SetGameValue(126, 10.0, 1);
    }

    if (ImGui::Button("Real gold lara"))
    {
        Game::PlayerTurnGold();
    }

    if (ImGui::Button("Give all weapons"))
    {
        for (int i = 0; i < 3; i++)
        {
            Game::InstancePost(player, 262256, i);
        }
    }

    static int item;
    ImGui::InputInt("itemId", &item);
    if (ImGui::Button("Give item"))
    {
        Game::InstancePost(player, 262256, item );
    }
#endif

#if TR7 && RETAIL_VERSION
    static int weapon;
    ImGui::InputInt("weapon", &weapon);
    if (ImGui::Button("Give weapon"))
    {
        Game::InstancePost(player, 262257, weapon);
    }
#endif

    // object select for birthing
    static char* name = nullptr;
    if (ImGui::BeginCombo("##Instance", name))
    {
        auto objectList = gObjectList;

        for (int i = 0; i < objectList->numObjects; i++)
        {
            auto object = &objectList->objects[i];
            auto selected = object->name == name;

            if (ImGui::Selectable(object->name, selected))
                name = object->name;

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("Birth instance"))
    {
        // check if object actually exists as file
        char drmName[256];
        LOAD_UnitFileName(drmName, name, "drm");

        if (name && GetFS()->FileExists(drmName))
        {
            cdc::Vector position;
            cdc::Vector rotation;
            if (player != nullptr)
            {
                position = player->position;
                rotation = player->rotation;
            }
            else
            {
                // player not found, try first instance
                position = (*(Instance**)INSTANCELIST)->position;
            }
#if TRAE
            auto unitId = *(int*)0x838418;
#elif TR7
            auto unitId = *(int*)(GAMETRACKER + 0xE8);
#elif TR8
            auto unitId = *(int*)((*(int*)PLAYERINSTANCE) + 224);
#elif ROTTR
            auto unitId = *(int*)((*(int*)PLAYERINSTANCE) + 224);
#endif

            auto tracker = Stream::GetObjectTrackerByName(name);
            while (tracker->status != 2 && Stream::PollLoadQueue());

            Game::BirthObjectNoParent(unitId, &position, &rotation, nullptr, tracker->object, 0, 1);
        }
    }

#if TRAE || TR7
    static char outfit[100] = "";
    ImGui::InputText("outfit", outfit, 100);
    if (ImGui::Button("Load outfit"))
    {
        switchPlayerNextFrame = true;

        auto obj = Game::GetObjectID(outfit);
        *(int*)(GAMETRACKER + 0x438) /* alt player object */ = obj;
    }
#endif

    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("Log", nullptr);

    // log window (based on imgui_demo log window)
    ImGui::BeginChild("LogRegion", ImVec2(), true);

    ImGui::TextUnformatted(this->logBuffer.begin());
    ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::End();
}

void Menu::DrawInstanceViewer(bool* show_instance_viewer)
{
    ImGui::Begin("Instances", show_instance_viewer);

    ImGui::Columns(2, "instances");
    ImGui::BeginChild("InstanceListTree");

    for (auto instance = *(Instance**)INSTANCELIST; instance != nullptr; instance = instance->next)
    {
        if (ImGui::TreeNodeEx((void*)instance, ImGuiTreeNodeFlags_Leaf, "%d %s", instance->introUniqueID, instance->object->name))
        {
            if (ImGui::IsItemClicked())
            {
                clickedInstance = (DWORD)instance;
            }

            ImGui::TreePop();
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    if (clickedInstance && *(int*)clickedInstance == 0)
    {
        // points to nullptr remove
        clickedInstance = 0;
    }

    if (clickedInstance)
    {
        auto oInstance = (Instance*)clickedInstance;

#if TRAE || TR7
        auto object = *(DWORD*)(clickedInstance + 0x94);
        auto data = *(DWORD*)(clickedInstance + 448);
        auto extraData = *(DWORD*)(clickedInstance + 572);

        auto numAnims = *(__int16*)(object + 0x1A);
        auto numModels = *(__int16*)(object + 0x18);

        auto name = (char*)*(DWORD*)(object + 0x48);
        auto intro = *(int*)(clickedInstance + 0x1D0);
#elif TR8
        auto object = *(DWORD*)(clickedInstance + 0x10);
        auto data = *(DWORD*)(clickedInstance + 80);

        auto numAnims = *(__int16*)(object + 0x38);
        auto numModels = *(__int16*)(object + 0x3A);

        auto name = (char*)*(DWORD*)(object + 0x60);
        auto intro = *(int*)(clickedInstance + 0x58);
#elif ROTTR
        // stand in data from TR8 to get pass compilation
        auto object = *(DWORD*)(clickedInstance + 0x10);
        auto data = *(DWORD*)(clickedInstance + 80);

        auto numAnims = *(__int16*)(object + 0x38);
        auto numModels = *(__int16*)(object + 0x3A);

        auto name = (char*)*(DWORD*)(object + 0x60);  
        auto intro = *(int*)(clickedInstance + 0x58);
#endif

        ImGui::Text("%s", name);

        auto coords = oInstance->position;
        auto rotation = oInstance->rotation;

        ImGui::Text("Position: %f %f %f", coords.x, coords.y, coords.z);
        ImGui::Text("Rotation: %f %f %f", rotation.x, rotation.y, rotation.z);

        ImGui::Text("Intro: %d", intro);

        if (data)
        {
            ImGui::Text("Family %d", *(unsigned __int16*)(data + 2));
        }

        ImGui::Text("Address %p", clickedInstance);

#if TRAE || TR7

        if (extraData)
        {
#if TRAE
            auto health = *(float*)(extraData + 5280);
#elif TR7
            auto health = *(float*)(extraData + 5040);
#endif
            ImGui::Text("Health: %8.2f", health);
        }

        if (ImGui::Button("Switch down"))
        {
            Game::InstancePost((Instance*)clickedInstance, 8388753, 1);
        }
        if (ImGui::Button("Switch up"))
        {
            Game::InstancePost((Instance*)clickedInstance, 8388753, 2);
        }
        ImGui::Text("Switch status: %d", Game::InstanceQuery((Instance*)clickedInstance, 233));
#endif

        auto player = *(Instance**)PLAYERINSTANCE;
        if (ImGui::Button("Goto"))
        {
            player->position = oInstance->position;
        }

        if (ImGui::Button("Bring"))
        {
#if TRAE || TR7
            oInstance->oldPosition = player->position;
#endif
            oInstance->position = player->position;
        }

        if (ImGui::Button("Delete"))
        {
            INSTANCE_ReallyRemoveInstance((Instance*)clickedInstance, 0, 0);
        }

        static int message;
        static int postdata;

        ImGui::InputInt("msg", &message);
        ImGui::InputInt("data", &postdata);
        if (ImGui::Button("Post"))
        {
            Game::InstancePost(oInstance, message, postdata);
        }

#if TRAE || TR7
        static int group;
        static int grouptoggle;
        ImGui::InputInt("drawgroup", &group);
        ImGui::InputInt("drawgrou toggle", &grouptoggle);

        if (ImGui::Button("Toggle drawgroup"))
        {
            INSTANCE_HideUnhideDrawGroup(oInstance, group, grouptoggle);
        }
#endif

        ImGui::Text("numModels %d", numModels);

        static int model;
        ImGui::InputInt("model index", &model);
        if (ImGui::Button("Set model"))
        {
            INSTANCE_SetModel(oInstance, model);
        }

        ImGui::Text("numAnims %d", numAnims);

        static int anim;
        ImGui::InputInt("anim", &anim);
        if (ImGui::Button("Play anim"))
        {
            G2EmulationInstanceSetAnimation(oInstance, 0, anim, 0, 0);
            G2EmulationInstanceSetMode(oInstance, 0, 2);
        }

#if TRAE
        if (ImGui::Button("Attach camera"))
        {
            void(__cdecl* CAMERA_SetInstanceFocus)(int a1, Instance* instance);
            CAMERA_SetInstanceFocus = reinterpret_cast<void(__cdecl*)(int a1, Instance * instance)>(0x00489380);

            CAMERA_SetInstanceFocus(0x850670, oInstance);
            *(cdc::Vector*)0x850670 = oInstance->position;

            auto cameraMode = (int*)0x850984;
            *cameraMode = 6;
        }
#endif

#if TR8
        if (ImGui::Button("Unhide"))
        {
            Game::InstancePost(oInstance, 7, 0);
        }

        static int maxList;

        if (ImGui::Button("List anims"))
        {
            auto animList = *(int*)(object + 0x44);
            for (int i = 0; i < maxList; i++)
            {
                auto anim = *(char**)(animList + (i * 0x0c) + 0x08);

                Hooking::GetInstance().GetMenu()->Log("%d %s\n", i, anim);
            }
        }
        ImGui::SameLine();
        ImGui::InputInt("num anims", &maxList);
#endif

#if TRAE || TR7
        auto modelList = *(int*)(object + 0x20);
        for (int i = 0; i < numModels; i++)
        {
            auto model = **(Model**)(modelList + (i * 4));

            char label[10];
            sprintf_s(label, "model %d", i);

            if (ImGui::CollapsingHeader(label))
            {
                ImGui::Text("version %d", model.version);
                ImGui::Text("numSegments %d", model.numSegments);
                ImGui::Text("numVirtSegments %d", model.numVirtSegments);
                ImGui::Text("numVertices %d", model.numVertices);
                ImGui::Text("numNormals %d", model.numNormals);
                ImGui::Text("numFaces %d", model.numFaces);
            }
        }
#endif
    }

    ImGui::End();
}

void Menu::DrawEventDebugViewer(bool* show_event_debug_viewer) const noexcept
{
    ImGui::Begin("Event debug", show_event_debug_viewer);

    auto level = *(Level**)(GAMETRACKER + 8);
    auto eventVarVals = *(int**)(GLOBALDATA + 0xE8 /* event vars */);

    if (level && level->reloc)
    {
        // get the level reloc module
        auto reloc = level->reloc;
        auto eventVars = reinterpret_cast<EventVar*>(RELOC_GetProcAddress(reloc, "EventDebug"));

        if (eventVars)
        {
            int i = 0;
            while (true)
            {
                auto var = eventVars[i++];

                if (var.name == nullptr)
                    break;

                ImGui::Text("%d %s = %d", var.offset, var.name, eventVarVals[var.offset]);
            }

            auto unsavedVars = reinterpret_cast<UnsavedVar*>(eventVars);
            while (true)
            {
                auto var = unsavedVars[i++];

                if (var.name == nullptr || var.var == nullptr)
                    break;

                ImGui::Text("%s %d", var.name, *var.var);
            }

            ImGui::Separator();
        }
    }

    static int var;
    static int value;
    ImGui::InputInt("eventVar", &var);
    ImGui::InputInt("value", &value);

    if (ImGui::Button("Set"))
    {
        eventVarVals[var] = value;
    }

    ImGui::End();
}

Instance* Menu::GetClickedInstance() noexcept
{
    return (Instance*)clickedInstance;
}

void Menu::Log(const char* fmt, ...) IM_FMTARGS(2)
{
    va_list args;
    va_start(args, fmt);
    this->logBuffer.appendfv(fmt, args);
    va_end(args);
}

bool Menu::IsFocus() const noexcept
{
    return m_focus;
}

FreeCameraMode Menu::GetFreeCamMode() const noexcept
{
    return m_freeCamMode;
}

void Menu::SetFocus(bool value) noexcept
{
    m_focus = value;

    // enable/disable game input
#if TRAE
    * (bool*)0x8551A9 = m_focus;
#elif TR8
    * (bool*)0xA02B79 = m_focus;
#elif TR7
    * (bool*)ADDR(0x110AF09, 0x1101689) = m_focus;
#endif
}

bool Menu::IsVisible() const noexcept
{
    return m_visible;
}

void Menu::SetVisibility(bool value) noexcept
{
    m_visible = value;
}

void Menu::OnLayoutChange() noexcept
{
    // thanks to Xwilarg
    auto hkl = GetKeyboardLayout(0);
    auto id = (int)((unsigned int)hkl & 0x0000FFFF);
    m_isAzertyLayout = id == 2060 || id == 1036;
}

void Menu::LoadSettings() noexcept
{
    HKEY phkResult;
    if (!RegOpenKeyA(HKEY_CURRENT_USER, "SOFTWARE\\TRAE-Menu-Hook", &phkResult))
    {
        DWORD value;
        DWORD len = sizeof(value);

        RegQueryValueExA(phkResult, "NoCinematicBars", 0, 0, (LPBYTE)&value, &len);
        m_drawSettings.noMovieBars = value;

        RegQueryValueExA(phkResult, "NoMotionBlur", 0, 0, (LPBYTE)&value, &len);
        m_drawSettings.noMotionBlur = value;

        RegCloseKey(phkResult);
    }
}

void Menu::SaveSettings() const noexcept
{
    HKEY phkResult;
    if (!RegCreateKeyA(HKEY_CURRENT_USER, "SOFTWARE\\TRAE-Menu-Hook", &phkResult))
    {
        DWORD value = m_drawSettings.noMovieBars;
        RegSetValueExA(phkResult, "NoCinematicBars", 0, REG_DWORD, (LPBYTE)&value, 4);

        value = m_drawSettings.noMotionBlur;
        RegSetValueExA(phkResult, "NoMotionBlur", 0, REG_DWORD, (LPBYTE)&value, 4);

        RegCloseKey(phkResult);
    }
}
