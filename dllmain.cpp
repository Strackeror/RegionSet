// dllmain.cpp : Définit le point d'entrée de l'application DLL.
#include <fstream>
#include <queue>
#include <functional>
#include <mutex>

#include <windows.h>

#include "minhook/MinHook.h"
#include "loader.h"
#include "ghidra_export.h"
#include "util.h"


using namespace loader;

static int overrideNextCallMax = -1;
static int overrideNextNotif = -1;

__declspec(dllexport) extern bool Load() {
  if (std::string(GameVersion) != "421631") {
    LOG(ERR) << "Region Set : Wrong version";
    return false;
  }

  MH_Initialize();

  HookLambda(MH::GuidingLands::AddRegionXp, [](longlong this_ptr, int region,
                                               int amount) {
    if (amount % 10000 == 0 && amount < 0) {
      auto currentXp = *(int *)(this_ptr + 0x269b30 + region * 4);
      int selectedLevel = -amount / 10000;

      overrideNextNotif = selectedLevel;
      auto targetXp = MH::GuidingLands::MaxXpForLevel(selectedLevel - 1);
      amount = targetXp - currentXp;
    }
    return original(this_ptr, region, amount);
  });

  HookLambda(MH::GuidingLands::LowerUI::Init_, [](longlong p1, longlong *p2) {
    auto player = MH::Player::GetPlayer(*(undefined **)MH::Player::BasePtr);
    overrideNextCallMax = *(player + 0x269c10 + *(int *)(p1 + 0x10));
    original(p1, p2);
  });

  HookLambda(MH::GuidingLands::LowerUI::Init,
             [](auto p1, auto p2, auto p3, auto p4, auto p5, auto p6, auto p7) {
               if (overrideNextCallMax > 0) {
                 p2 = overrideNextCallMax;
                 p7 = overrideNextCallMax;
                 p4 = 0;
                 overrideNextCallMax = -1;
               }
               original(p1, p2, p3, p4, p5, p6, p7);
             });

  HookLambda(MH::GuidingLands::RegionLevelChangeNotif,
    [](auto p1, auto p2, auto p3, auto p4) {
      if (overrideNextNotif >= 0) {
        p4 = overrideNextNotif;
        overrideNextNotif = -1;
      }
      original(p1, p2, p3, p4);
    });

  MH_ApplyQueued();

  return true;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		return Load();
    return TRUE;
}

