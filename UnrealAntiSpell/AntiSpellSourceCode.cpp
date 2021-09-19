#define INI_USE_STACK 0

#include <Windows.h>
#include <string>
#include <vector>
#include <time.h>


#include <string>
#include <sstream>
#include <iostream>
#include <process.h>
#include <TlHelp32.h>


#include "ini.h"
#include "verinfo.h"
#include "INIReader.h"
#include "IniWriter.h"

INIReader reader = INIReader();
CIniWriter writer = CIniWriter();
#define IsKeyPressed(CODE) (GetAsyncKeyState(CODE) & 0x8000) > 0


int GameDll = 0;
int pJassEnvAddress = 0;

HMODULE MainModule = NULL;
HANDLE MainThread = NULL;
LPVOID TlsValue;
DWORD TlsIndex;
DWORD _W3XTlsIndex;



DWORD GetIndex()
{
	return *(DWORD*)(_W3XTlsIndex);
}

DWORD GetW3TlsForIndex(DWORD index)
{
	DWORD pid = GetCurrentProcessId();
	THREADENTRY32 te32;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
	te32.dwSize = sizeof(THREADENTRY32);

	if (Thread32First(hSnap, &te32))
	{
		do
		{
			if (te32.th32OwnerProcessID == pid)
			{
				HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, false, te32.th32ThreadID);
				CONTEXT ctx = { CONTEXT_SEGMENTS };
				LDT_ENTRY ldt;
				GetThreadContext(hThread, &ctx);
				GetThreadSelectorEntry(hThread, ctx.SegFs, &ldt);
				DWORD dwThreadBase = ldt.BaseLow | (ldt.HighWord.Bytes.BaseMid <<
					16) | (ldt.HighWord.Bytes.BaseHi << 24);
				CloseHandle(hThread);
				if (dwThreadBase == NULL)
					continue;
				DWORD* dwTLS = *(DWORD**)(dwThreadBase + 0xE10 + 4 * index);
				if (dwTLS == NULL)
					continue;
				return (DWORD)dwTLS;
			}
		} while (Thread32Next(hSnap, &te32));
	}

	return NULL;
}

void SetTlsForMe()
{
	TlsIndex = GetIndex();
	LPVOID tls = (LPVOID)GetW3TlsForIndex(TlsIndex);
	TlsValue = tls;
}
typedef int(__cdecl * pGetTriggerUnit)();
pGetTriggerUnit GetTriggerUnit;
typedef int(__cdecl * pGetSpellTargetUnit)();
pGetSpellTargetUnit GetSpellTargetUnit;
typedef float(__cdecl * pGetSpellTargetX)();
pGetSpellTargetX GetSpellTargetX;
typedef float(__cdecl * pGetSpellTargetY)();
pGetSpellTargetY GetSpellTargetY;

typedef int(__cdecl * pGetTriggerEventId)();
pGetTriggerEventId GetTriggerEventId_real;
typedef int(__cdecl * pGetSpellAbilityId)();
pGetSpellAbilityId GetSpellAbilityId_real;
typedef int(__cdecl * pGetIssuedOrderId)();
pGetIssuedOrderId GetIssuedOrderId_real;

DWORD LatestCheck = 0;

int pRecoveryJassNative1 = 0;

int MapNameOffset1 = 0;
int MapNameOffset2 = 0;

const char * GetCurrentMapName()
{
	int offset1 = *(int*)MapNameOffset1;
	if (offset1 > 0)
	{
		return (const char *)(offset1 + MapNameOffset2);
	}
	return 0;
}

bool UpdatingSpellList = false;

enum TargetType : unsigned char
{
	TARGET_UNIT_ENEMY = 1,
	TARGET_UNIT_ALLY = 2,
	TARGET_POINT_WITH_ENEMY = 3,
	TARGET_POINT_WITH_ALLY = 4,
	TARGET_NONE = 5
};

enum AvoidTriggger : unsigned char
{
	AVOID_AFTER_CAST = 1,
	AVOID_BEFORE_CAST = 2
};

enum AvoidType : unsigned char
{
	AVOIDTYPE_MOVE = 1,
	AVOIDTYPE_MAGIC_SHIELD = 2,
	AVOIDTYPE_BLINK = 3,
	AVOIDTYPE_INVISIBLE = 4
};


struct Spell
{
	int				SpellID;
	int				OrderID;
	bool			IsItem;
	TargetType		Target;
	AvoidTriggger	Trigger;
	AvoidType		Type;
	bool			NeedAvoid;
	bool			UsedForAvoidSkills;
	float			Distance[4];
};


std::vector<Spell> SpellList;


int GetDefAddr(int id, int first, int currentOffset, bool reversedAllowed)
{
	int FirstDataDefEntry = *(int*)first;
	int CurrentDefAddr = *(int*)(FirstDataDefEntry + 8) + currentOffset;
	int FirstDefAddr = CurrentDefAddr;
	int CurrentDefId = 0;
	int CurrentDefId2 = 0;
	if (FirstDataDefEntry == 0)
		return 0;

	while (true)
	{

		CurrentDefId = *(int*)(CurrentDefAddr + 8);
		if (reversedAllowed)
			CurrentDefId2 = *(int*)(CurrentDefAddr + 12);


		if (CurrentDefId == id || (reversedAllowed || CurrentDefId2 == id))
			break;

		CurrentDefAddr = *(int*)(CurrentDefAddr);

		if (CurrentDefAddr == 0 || CurrentDefAddr == FirstDefAddr)
			return 0;
	}

	if (CurrentDefAddr == 0)
		return 0;

	return CurrentDefAddr - 0xC;
}

int pAbilityUIDefAddr = 0;
int pAbilityDataDefAddr = 0;

int GetAbilityUIdata(int id)
{
	return GetDefAddr(id, pAbilityDataDefAddr, 0x10, false);
}

int GetAbilityData(int id)
{
	return GetDefAddr(id, pAbilityDataDefAddr, 0xC, false);
}





typedef int(__fastcall * p_GetTypeInfo)(int unit_item_code, int unused);
p_GetTypeInfo GetTypeInfo = NULL;


char * GetObjectNameByID(int clid)
{
	if (clid > 0)
	{
		int tInfo = GetTypeInfo(clid, 0);
		int tInfo_1d, tInfo_2id;
		if (tInfo && (tInfo_1d = *(int *)(tInfo + 40)) != 0)
		{
			tInfo_2id = tInfo_1d - 1;
			if (tInfo_2id >= (unsigned int)0)
				tInfo_2id = 0;
			return (char *)*(int *)(*(int *)(tInfo + 44) + 4 * tInfo_2id);
		}
		else
		{
			return "Default String";
		}
	}
	return "Default String";
}

int GetObjectClassID(int unit_or_item_addr)
{
	if (unit_or_item_addr)
		return *(int*)(unit_or_item_addr + 0x30);
	return 0;
}

char * GetObjectName(int objaddress)
{
	return GetObjectNameByID(GetObjectClassID(objaddress));
}

typedef int(__fastcall * pGetHandleUnitAddress) (int HandleID, int unused);
pGetHandleUnitAddress GetHandleUnitAddress;
pGetHandleUnitAddress GetHandleItemAddress;


void PrintNewAbility(int AbilID)
{

}


int GetSpellAbilityId_hooked()
{
	if (!MainThread)
		MainThread = GetCurrentThread();

	int ReturnValue = GetSpellAbilityId_real();
	int IssueOrderId = GetIssuedOrderId_real();
	if (ReturnValue > 0 && IssueOrderId > 0)
	{
		if (GetTickCount() > LatestCheck + 100)
		{
			LatestCheck = GetTickCount();
			if (!UpdatingSpellList)
			{



				//

				int TriggerEventID = GetTriggerEventId_real();
				int hCaster = GetTriggerUnit();
				int hTarget = GetSpellTargetUnit();





			}
		}
	}

	return ReturnValue;
}


int CreateJassNativeHook(int oldaddress, int newaddress)
{
	int FirstAddress = *(int *)pJassEnvAddress;
	if (FirstAddress)
	{
		FirstAddress = *(int *)(FirstAddress + 20);
		if (FirstAddress)
		{

			FirstAddress = *(int *)(FirstAddress + 32);
			if (FirstAddress)
			{

				int NextAddress = FirstAddress;

				while (TRUE)
				{
					if (*(int *)(NextAddress + 12) == oldaddress)
					{
						*(int *)(NextAddress + 12) = newaddress;

						return NextAddress + 12;
					}

					NextAddress = *(int*)NextAddress;

					if (NextAddress == FirstAddress || NextAddress <= 0)
						break;
				}
			}
		}

	}
	return 0;
}

char LatestMap[MAX_PATH];

DWORD WINAPI  WaitForNativeReset(LPVOID)
{
	MessageBox(0, "Start!", "Start!", 0);
	TlsSetValue(TlsIndex, TlsValue);

	while (true)
	{
		if (GetCurrentMapName() != NULL)
		{
			MessageBox(0, GetCurrentMapName(), "Found map name!",0);

			UpdatingSpellList = false;
			int pOldRecoveryJassNative1 = pRecoveryJassNative1;
			pRecoveryJassNative1 = CreateJassNativeHook((int)GetSpellAbilityId_real, (int)&GetSpellAbilityId_hooked);
			if (pRecoveryJassNative1 == 0 && _stricmp(LatestMap, GetCurrentMapName()) == 0)
				pRecoveryJassNative1 = pOldRecoveryJassNative1;
			else
			{
				UpdatingSpellList = true;


				SpellList.clear();

				MessageBox(0, (std::string(GetCurrentMapName()) + std::string("_AntiSpell.ini")).c_str(), "Need create config!", 0);

				writer = CIniWriter(std::string(GetCurrentMapName()) + std::string("_AntiSpell.ini"));
				writer.WriteString("MAIN", "Author", "Absol");
				reader = INIReader(std::string(GetCurrentMapName()) + std::string("_AntiSpell.ini"));
				

				UpdatingSpellList = false;
			}
		}
		else UpdatingSpellList = true;

		Sleep(200);
	}
	return 0;
}

void InitHooksAndTls()
{
	SetTlsForMe();
	TlsSetValue(TlsIndex, TlsValue);

}


void Init26a()
{
	_W3XTlsIndex = 0xAB7BF4 + GameDll;
	pJassEnvAddress = GameDll + 0xADA848;
	GetSpellAbilityId_real = (pGetSpellAbilityId)(GameDll + 0x3C32A0);
	GetTriggerEventId_real = (pGetTriggerEventId)(GameDll + 0x3BB2C0);
	GetSpellTargetUnit = (pGetSpellTargetUnit)(GameDll + 0x3C3A80);
	GetSpellTargetX = (pGetSpellTargetX)(GameDll + 0x3C3580);
	GetSpellTargetY = (pGetSpellTargetY)(GameDll + 0x3C3670);
	GetTriggerUnit = (pGetTriggerUnit)(GameDll + 0x3BB240);
	GetTypeInfo = (p_GetTypeInfo)(GameDll + 0x32C880);
	pAbilityUIDefAddr = GameDll + 0xAB5918;
	pAbilityDataDefAddr = GameDll + 0xAB3E64;
	GetHandleUnitAddress = (pGetHandleUnitAddress)(GameDll + 0x3BDCB0);
	GetHandleItemAddress = (pGetHandleUnitAddress)(GameDll + 0x3BEB50);
	GetIssuedOrderId_real = (pGetIssuedOrderId)(GameDll + 0x3C2C80);
	MapNameOffset1 = GameDll + 0xAAE788;
	MapNameOffset2 = 8;


	InitHooksAndTls();
	
	CreateThread(0, 0, WaitForNativeReset, 0, 0, 0);

	
}

void Init27a()
{
	_W3XTlsIndex = 0xBB8628 + GameDll;
	pJassEnvAddress = GameDll + 0xBE3740;
	GetSpellAbilityId_real = (pGetSpellAbilityId)(GameDll + 0x1E4DD0);
	GetTriggerEventId_real = (pGetTriggerEventId)(GameDll + 0x1E5D90);
	GetSpellTargetUnit = (pGetSpellTargetUnit)(GameDll + 0x1E52B0);
	GetSpellTargetX = (pGetSpellTargetX)(GameDll + 0x1E53E0);
	GetSpellTargetY = (pGetSpellTargetY)(GameDll + 0x1E54C0);
	GetTriggerUnit = (pGetTriggerUnit)(GameDll + 0x1E5E10);
	GetTypeInfo = (p_GetTypeInfo)(GameDll + 0x327020);
	pAbilityUIDefAddr = GameDll + 0xBE6158;
	pAbilityDataDefAddr = GameDll + 0xBECD44;
	GetHandleUnitAddress = (pGetHandleUnitAddress)(GameDll + 0x1D1550);
	GetHandleItemAddress = (pGetHandleUnitAddress)(GameDll + 0x1CFC50);
	GetIssuedOrderId_real = (pGetIssuedOrderId)(GameDll + 0x1E2B60);
	MapNameOffset1 = GameDll + 0xBEE150;
	MapNameOffset2 = 8;



	InitHooksAndTls();
	
	CreateThread(0, 0, WaitForNativeReset, 0, 0, 0);
	

}


void __cdecl InitializeAntiSkill(LPVOID)
{
	HMODULE hGameDll = GetModuleHandle("Game.dll");
	if (!hGameDll)
	{
		MessageBox(0, "AntiSkill problem!\nNo game.dll found.", "Game.dll not found", 0);
		return;
	}

	GameDll = (int)hGameDll;

	CFileVersionInfo gdllver;
	gdllver.Open(hGameDll);
	// Game.dll version (1.XX)
	int GameDllVer = gdllver.GetFileVersionQFE();
	gdllver.Close();

	if (GameDllVer == 6401)
	{
		Init26a();
	}
	else if (GameDllVer == 52240)
	{
		Init27a();
	}
	else
	{
		MessageBox(0, "AntiSkill problem!\nGame version not supported.", "\nGame version not supported", 0);
		return;
	}


}



BOOL __stdcall DllMain(HINSTANCE i, DWORD r, LPVOID x)
{
	if (r == DLL_PROCESS_ATTACH)
	{
		MainModule = i;
	
		InitializeAntiSkill( 0);
	}
	else if (r == DLL_PROCESS_DETACH)
	{

	}

	return TRUE;
}