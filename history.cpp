#include "stdafx.h"
#include "common.h"
#include "history.h"
#include "damage.h"
#include "tig/tig_mes.h"




class HistSysReplacements : public TempleFix
{
	macTempleFix(History System)
	{
		
	}
} historySysReplacements;


struct HistorySystemAddresses : AddressTable
{

	int32_t *rollSerialNumber; 
	MesHandle * rollUiMesHandle;
	HistoryArrayEntry * histArray;
	int (__cdecl *RollHistoryType1Add)(objHndl objHnd, objHndl objHnd2, DamagePacket *damPkt);
	int(__cdecl *RollHistoryType2Add)(objHndl objHnd, objHndl objHnd2, int skillIdx, int dicePacked, int rollResult, int DC, BonusList *bonlist);
	int (__cdecl *RollHistoryType3Add)(objHndl obj, int DC, int saveType, int flags, int dicePacked, int rollResult, BonusList *bonListIn);
	HistorySystemAddresses()
	{
		rebase(rollUiMesHandle, 0x102B0168);
		rebase(rollSerialNumber, 0x102B016C);
		rebase(RollHistoryType1Add, 0x10047C80);
		rebase(RollHistoryType2Add, 0x10047CF0);
		rebase(RollHistoryType3Add, 0x10047D90);
		rebase(histArray, 0x109DDA20);
	}
} addresses;



#pragma region History System Implementation
HistorySystem histSys;


HistorySystem::HistorySystem()
{
	rebase(RollHistoryAdd, 0x10047430);
}
#pragma endregion

#pragma region Replacements


#pragma endregion


struct HistoryEntry
{
	uint32_t histId;
	uint32_t histType;
	objHndl obj;
	ObjectId objId;
	char objDescr[2000];
	objHndl obj2;
	ObjectId obj2Id;
	char obj2Descr[2000];
	uint32_t prevId;
	uint32_t nextId;
};

struct HistoryArrayEntry
{
	uint32_t field0;
	uint32_t field4;
	uint32_t histId;
	uint32_t histType;
	objHndl obj;
	ObjectId objId;
	char objDescr[2000];
	objHndl obj2;
	ObjectId obj2Id;
	char obj2Descr[2000];
	uint32_t prevId;
	uint32_t nextId;
	uint32_t pad[326];
};

struct HistoryEntryType1 : HistoryEntry
{
	DamagePacket dmgPkt;
};


struct HistoryEntryType2 : HistoryEntry
{
	uint32_t dicePacked;
	int32_t rollResult;
	uint32_t skillIdx;
	uint32_t dc;
	BonusList bonlist;
	uint32_t pad[100];
};

struct HistoryEntryType3 : HistoryEntry
{
	uint32_t dicePacked;
	int32_t rollResult;
	uint32_t dc;
	uint32_t saveType;
	uint32_t saveFlags;
	BonusList bonlist;
	uint32_t pad[99];
};

const auto TestSizeOfHistoryEntryType1 = sizeof(HistoryEntryType1); // should be 5384 (0x1508)
const auto TestSizeOfHistoryEntryType2 = sizeof(HistoryEntryType2); // should be 5384 (0x1508)
const auto TestSizeOfHistoryEntryType3 = sizeof(HistoryEntryType3); // should be 5384 (0x1508)
const auto TestSizeOfHistoryArrayEntry = sizeof(HistoryArrayEntry); // should be 5392 (0x1510)