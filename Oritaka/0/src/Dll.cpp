#include <windows.h>
#include <BWAPI.h>
#include <BWTA.h>
#include "Oritaka.h"


BWAPI::Game* BWAPI::Broodwar;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{   
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			BWAPI::BWAPI_init();
			break;
		case DLL_PROCESS_DETACH:
			break;
	}

	return TRUE;
}

extern "C" __declspec(dllexport) BWAPI::AIModule* newAIModule(BWAPI::Game* game)
{
  BWAPI::Broodwar = game;
  return new Oritaka;
}