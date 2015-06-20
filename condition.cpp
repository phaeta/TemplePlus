#include "stdafx.h"
#include "common.h"
#include "dispatcher.h"
#include "condition.h"
#include "temple_functions.h"
#include "obj.h"
#include "bonus.h"
#include "radialmenu.h"
#include "combat.h"
#include "critter.h"

ConditionSystem conds;
CondStructNew conditionDisableAoO;

struct ConditionSystemAddresses : AddressTable
{
	void(__cdecl* SetPermanentModArgsFromDataFields)(Dispatcher* dispatcher, CondStruct* condStruct, int* condArgs);
	ConditionSystemAddresses()
	{
		rebase(SetPermanentModArgsFromDataFields, 0x100E1B90);
	}
} addresses;

class ConditionFunctionReplacement : public TempleFix {
public:
	const char* name() override {
		return "Condition Function Replacement";
	}

	void apply() override {
		logger->info("Replacing Condition-related Functions");
		
		replaceFunction(0x100E19C0, _CondStructAddToHashtable);
		replaceFunction(0x100E1A80, _GetCondStructFromHashcode);
		replaceFunction(0x100E1AB0, _CondNodeGetArg);
		replaceFunction(0x100E1AD0, _CondNodeSetArg);
		replaceFunction(0x100E1DD0, _CondNodeAddToSubDispNodeArray);

		replaceFunction(0x100E22D0, _ConditionAddDispatch);
		replaceFunction(0x100E24C0, _ConditionAddToAttribs_NumArgs0);
		replaceFunction(0x100E2500, _ConditionAddToAttribs_NumArgs2);
		replaceFunction(0x100E24E0, _ConditionAdd_NumArgs0);
		replaceFunction(0x100E2530, _ConditionAdd_NumArgs2);
		replaceFunction(0x100E2560, _ConditionAdd_NumArgs3);
		replaceFunction(0x100E2590, _ConditionAdd_NumArgs4);
		replaceFunction(0x100E25C0, InitCondFromCondStructAndArgs);
		replaceFunction(0x100ECF30, ConditionPrevent);
		replaceFunction(0x100EE280, GlobalToHitBonus);
		replaceFunction(0x10101150, SkillBonusCallback);
		
		replaceFunction(0x100F7B60, _FeatConditionsRegister);
		replaceFunction(0x100F7BE0, _GetCondStructFromFeat);
		
		
	}
} condFuncReplacement;


CondNode::CondNode(CondStruct *cond) {
	memset(this, 0, sizeof(CondNode));
	condStruct = cond;
}


#pragma region Condition Add Functions

int32_t _CondNodeGetArg(CondNode* condNode, uint32_t argIdx)
{
	return conds.CondNodeGetArg(condNode, argIdx);
}

void _CondNodeSetArg(CondNode* condNode, uint32_t argIdx, uint32_t argVal)
{
	conds.CondNodeSetArg(condNode, argIdx, argVal);
}

uint32_t _ConditionAddDispatch(Dispatcher* dispatcher, CondNode** ppCondNode, CondStruct* condStruct, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
	assert(condStruct->numArgs >= 0 && condStruct->numArgs <= 6);

	vector<int> args;
	if (condStruct->numArgs > 0) {
		args.push_back(arg1);
	}
	if (condStruct->numArgs > 1) {
		args.push_back(arg2);
	}
	if (condStruct->numArgs > 2) {
		args.push_back(arg3);
	}
	if (condStruct->numArgs > 3) {
		args.push_back(arg4);
	}


	return _ConditionAddDispatchArgs(dispatcher, ppCondNode, condStruct, args);
};

uint32_t _ConditionAddDispatchArgs(Dispatcher* dispatcher, CondNode** ppCondNode, CondStruct* condStruct, const vector<int> &args) {
	assert(condStruct->numArgs >= args.size());

	// pre-add section (may abort adding condition, or cause another condition to be deleted first)
	DispIoCondStruct dispIO14h;
	dispIO14h.dispIOType = dispIoTypeCondStruct;
	dispIO14h.condStruct = condStruct;
	dispIO14h.outputFlag = 1;
	dispIO14h.arg1 = 0;
	dispIO14h.arg2 = 0;
	if (args.size() > 0) {
		dispIO14h.arg1 = args[0];
	}
	if (args.size() > 1) {
		dispIO14h.arg2 = args[1];
	}

	_DispatcherProcessor(dispatcher, dispTypeConditionAddPre, 0, (DispIO*)&dispIO14h);

	if (dispIO14h.outputFlag == 0) {
		return 0;
	}

	// adding condition
	auto condNodeNew = new CondNode(condStruct);
	for (unsigned int i = 0; i < condStruct->numArgs; ++i) {
		if (i < args.size()) {
			condNodeNew->args[i] = args[i];
		} else {
			// Fill the rest with zeros
			condNodeNew->args[i] = 0;
		}
	}

	CondNode** ppNextCondeNode = ppCondNode;

	while (*ppNextCondeNode != nullptr) {
		ppNextCondeNode = &(*ppNextCondeNode)->nextCondNode;
	}
	*ppNextCondeNode = condNodeNew;

	_CondNodeAddToSubDispNodeArray(dispatcher, condNodeNew);


	auto dispatcherSubDispNodeType1 = dispatcher->subDispNodes[1];
	while (dispatcherSubDispNodeType1 != nullptr) {
		if (dispatcherSubDispNodeType1->subDispDef->dispKey == 0
			&& (dispatcherSubDispNodeType1->condNode->flags & 1) == 0
			&& condNodeNew == dispatcherSubDispNodeType1->condNode) {
			dispatcherSubDispNodeType1->subDispDef->dispCallback(dispatcherSubDispNodeType1, dispatcher->objHnd, dispTypeConditionAdd, 0, nullptr);
		}

		dispatcherSubDispNodeType1 = dispatcherSubDispNodeType1->next;
	}

	return 1;
};

void _CondNodeAddToSubDispNodeArray(Dispatcher* dispatcher, CondNode* condNode) {
	auto subDispDef = condNode->condStruct->subDispDefs;

	while (subDispDef->dispType != 0) {
		auto subDispNodeNew = (SubDispNode *)allocFuncs._malloc_0(sizeof(SubDispNode));
		subDispNodeNew->subDispDef = subDispDef;
		subDispNodeNew->next = nullptr;
		subDispNodeNew->condNode = condNode;


		auto dispType = subDispDef->dispType;
		assert(dispType >= 0 && dispType < dispTypeCount);

		auto ppDispatcherSubDispNode = &(dispatcher->subDispNodes[dispType]);

		if (*ppDispatcherSubDispNode != nullptr) {
			while ((*ppDispatcherSubDispNode)->next != nullptr) {
				ppDispatcherSubDispNode = &((*ppDispatcherSubDispNode)->next);
			}
			(*ppDispatcherSubDispNode)->next = subDispNodeNew;
		}
		else {
			dispatcher->subDispNodes[subDispDef->dispType] = subDispNodeNew;
		}


		subDispDef += 1;
	}

};


uint32_t _ConditionAddToAttribs_NumArgs0(Dispatcher* dispatcher, CondStruct* condStruct) {
	return _ConditionAddDispatch(dispatcher, &dispatcher->permanentMods, condStruct, 0, 0, 0, 0);
};

uint32_t _ConditionAddToAttribs_NumArgs2(Dispatcher* dispatcher, CondStruct* condStruct, uint32_t arg1, uint32_t arg2) {
	return _ConditionAddDispatch(dispatcher, &dispatcher->permanentMods, condStruct, arg1, arg2, 0, 0);
};

uint32_t _ConditionAdd_NumArgs0(Dispatcher* dispatcher, CondStruct* condStruct) {
	return _ConditionAddDispatch(dispatcher, &dispatcher->conditions, condStruct, 0, 0, 0, 0);
};

uint32_t _ConditionAdd_NumArgs1(Dispatcher* dispatcher, CondStruct* condStruct, uint32_t arg1) {
	return _ConditionAddDispatch(dispatcher, &dispatcher->conditions, condStruct, arg1, 0, 0, 0);
};

uint32_t _ConditionAdd_NumArgs2(Dispatcher* dispatcher, CondStruct* condStruct, uint32_t arg1, uint32_t arg2) {
	return _ConditionAddDispatch(dispatcher, &dispatcher->conditions, condStruct, arg1, arg2, 0, 0);
};

uint32_t _ConditionAdd_NumArgs3(Dispatcher* dispatcher, CondStruct* condStruct, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
	return _ConditionAddDispatch(dispatcher, &dispatcher->conditions, condStruct, arg1, arg2, arg3, 0);
};

uint32_t _ConditionAdd_NumArgs4(Dispatcher* dispatcher, CondStruct* condStruct, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
	return _ConditionAddDispatch(dispatcher, &dispatcher->conditions, condStruct, arg1, arg2, arg3, arg4);
}

void InitCondFromCondStructAndArgs(Dispatcher* dispatcher, CondStruct* condStruct, int* condargs)
{
	conds.InitCondFromCondStructAndArgs(dispatcher, condStruct, condargs);
};

#pragma endregion

uint32_t ConditionPrevent(DispatcherCallbackArgs args)
{
	DispIoCondStruct * dispIO = _DispIoCheckIoType1((DispIoCondStruct*)args.dispIO);
	if (dispIO == nullptr)
	{
		logger->error("Dispatcher Error! Condition {} fuckup, wrong DispIO type", args.subDispNode->condNode->condStruct->condName);
		return 0; // if we get here then VERY BAD!
	}
	if (dispIO->condStruct == (CondStruct *)args.subDispNode->subDispDef->data1)
	{
		dispIO->outputFlag = 0;
	}
	return 0;
};

int __cdecl SkillBonusCallback(DispatcherCallbackArgs args)
{
	/*
	used by conditions: Skill Circumstance Bonus, Skill Competence Bonus
	*/
	SkillEnum skillEnum = (SkillEnum)conds.CondNodeGetArg(args.subDispNode->condNode, 0);
	int bonValue = conds.CondNodeGetArg(args.subDispNode->condNode, 1);
	int bonType = args.subDispNode->subDispDef->data1;
	if (args.dispKey - 20 == skillEnum)
	{
		int invIdx = conds.CondNodeGetArg(args.subDispNode->condNode, 2);
		objHndl itemHnd = inventory.GetItemAtInvIdx(args.objHndCaller, invIdx);
		DispIoBonusAndObj * dispIo = dispatch.DispIoCheckIoType10((DispIoBonusAndObj*)args.dispIO);
		const char * name = description.getDisplayName(itemHnd, args.objHndCaller);
		bonusSys.bonusAddToBonusListWithDescr(dispIo->bonOut, bonValue, bonType, 112, (char*)name);
	}
	return 0;
}

int __cdecl GlobalToHitBonus(DispatcherCallbackArgs args)
{
	DispIoAttackBonus * dispIo = dispatch.DispIoCheckIoType5((DispIoAttackBonus*)args.dispIO);
	dispatch.DispatchToHitBonusBase(args.objHndCaller, dispIo);

	// natural attack - get attack bonus from internal defs
	if (dispIo->attackPacket.dispKey >= (ATTACK_CODE_NATURAL_ATTACK+1) 
		 && !d20Sys.d20Query(args.objHndCaller, DK_QUE_Polymorphed) )
	{
		int attackIdx = dispIo->attackPacket.dispKey - (ATTACK_CODE_NATURAL_ATTACK + 1);
		int bonValue = 0; // temporarily used as an index value for obj_f_attack_bonus_idx field
		for (int i = 0,  j=0; i < 4; i++)
		{
			j += objects.getArrayFieldInt32(args.objHndCaller, obj_f_critter_attacks_idx, i); // number of attacks
			if (attackIdx < j){
				bonValue = i;
				break;
			}
		}
		bonValue = objects.getArrayFieldInt32(args.objHndCaller, obj_f_attack_bonus_idx, bonValue);
		bonusSys.bonusAddToBonusList(&dispIo->bonlist, bonValue, 1, 118); // base attack
	}

	if (dispIo->attackPacket.flags & D20CAF_RANGED) // get to hit modifier from DEX
	{
		bonusSys.bonusAddToBonusList(&dispIo->bonlist, 
			objects.GetModFromStatLevel(objects.abilityScoreLevelGet(args.objHndCaller, stat_dexterity, 0)) , 3, 104);
	} else // get to hit mod from STR
	{
		bonusSys.bonusAddToBonusList(&dispIo->bonlist,
			objects.GetModFromStatLevel(objects.abilityScoreLevelGet(args.objHndCaller, stat_strength, 0)), 2, 103);
	}

	int attackCode = dispIo->attackPacket.dispKey;
	if (attackCode < ATTACK_CODE_NATURAL_ATTACK) // apply penalties for Nth attack
	{
		int attackNumber = 1;
		int dualWielding = 0;
		d20Sys.ExtractAttackNumber(args.objHndCaller, attackCode, &attackNumber, &dualWielding);
		assert(attackNumber > 0);
		switch (attackNumber)
		{
		case 1: 
			break;
		case 2:
			bonusSys.bonusAddToBonusList(&dispIo->bonlist, -(attackNumber-1) * 5, 24, 119);
			break;
		default:
			bonusSys.bonusAddToBonusList(&dispIo->bonlist, -(attackNumber - 1) * 5, 25, 120);
		}
		if (dualWielding)
		{
			if (d20Sys.UsingSecondaryWeapon(args.objHndCaller, attackCode))
				bonusSys.bonusAddToBonusList(&dispIo->bonlist, -10, 26, 121); // penalty for dualwield on offhand attack
			else
				bonusSys.bonusAddToBonusList(&dispIo->bonlist, -6, 27, 122); // penalty for dualwield on primary attack

			auto offhand = inventory.ItemWornAt(args.objHndCaller, 4);
			if (offhand)
			{
				if (inventory.GetWieldType(dispIo->attackPacket.attacker, offhand) == 0)
					bonusSys.bonusAddToBonusList(&dispIo->bonlist, 2, 0, 167); // Light Off-hand Weapon
			}
		}
	}

	// helplessness bonus
	if (dispIo->attackPacket.victim
		&& d20Sys.d20Query(dispIo->attackPacket.victim, DK_QUE_Helpless)
		&& !d20Sys.d20Query(dispIo->attackPacket.victim, DK_QUE_Critter_Is_Stunned))
		bonusSys.bonusAddToBonusList(&dispIo->bonlist, 4, 30, 136);
	
	// flanking bonus
	if (combatSys.IsFlankedBy(dispIo->attackPacket.victim, dispIo->attackPacket.attacker))
	{
		bonusSys.bonusAddToBonusList(&dispIo->bonlist, 2, 0, 201);
		dispIo->attackPacket.flags |= D20CAF_FLANKED;
	}

	// size bonus / penalty
	int sizeCategory = dispatch.DispatchGetSizeCategory(args.objHndCaller);
	int sizeCatBonus = critterSys.GetBonusFromSizeCategory(sizeCategory);
	bonusSys.bonusAddToBonusList(&dispIo->bonlist, sizeCatBonus, 0, 115);

	return 0;
}

void _FeatConditionsRegister()
{
	conds.hashmethods.CondStructAddToHashtable(conds.ConditionAttackOfOpportunity);
	conds.hashmethods.CondStructAddToHashtable(conds.ConditionCastDefensively);
	conds.hashmethods.CondStructAddToHashtable(conds.ConditionCombatCasting);
	conds.hashmethods.CondStructAddToHashtable(conds.ConditionDealSubdualDamage );
	conds.hashmethods.CondStructAddToHashtable(conds.ConditionDealNormalDamage);
	conds.hashmethods.CondStructAddToHashtable(conds.ConditionFightDefensively);
	conds.hashmethods.CondStructAddToHashtable(conds.ConditionAnimalCompanionAnimal);
	conds.hashmethods.CondStructAddToHashtable(conds.ConditionAutoendTurn);
	conds.hashmethods.CondStructAddToHashtable((CondStruct*)conds.mConditionDisableAoO); //NEW!
	for (unsigned int i = 0; i < 84; i++)
	{
		conds.hashmethods.CondStructAddToHashtable(conds.FeatConditionDict[i].condStruct);
	}
}

uint32_t  _GetCondStructFromFeat(feat_enums featEnum, CondStruct ** condStructOut, uint32_t * arg2Out)
{
	feat_enums * featFromDict = & ( conds.FeatConditionDict->featEnum );
	uint32_t iter = 0;
	while (
		( (int32_t)featEnum != featFromDict[0] || featFromDict[1] != -1)
		&&  ( (int32_t)featEnum < (int32_t)featFromDict[0] 
				|| (int32_t)featEnum >= (int32_t)featFromDict[1]  )
		)
	{
		iter += 16;
		featFromDict += 4;
		if (iter >= 0x540){ return 0; }
	}

	*condStructOut = (CondStruct *)*(featFromDict - 1);
	*arg2Out = featEnum + featFromDict[2] - featFromDict[0];
	return 1;
}

uint32_t _CondStructAddToHashtable(CondStruct * condStruct)
{
	return conds.hashmethods.CondStructAddToHashtable(condStruct);
}

CondStruct * _GetCondStructFromHashcode(uint32_t key)
{
	return conds.hashmethods.GetCondStruct(key);
}

CondStruct* ConditionSystem::GetByName(const string& name) {
	auto key = templeFuncs.StringHash(name.c_str());
	return hashmethods.GetCondStruct(key);
}

void ConditionSystem::AddToItem(objHndl item, const CondStruct* cond, const vector<int>& args) {
	assert(args.size() == cond->numArgs);

	auto curCondCount = templeFuncs.Obj_Get_IdxField_NumItems(item, obj_f_item_pad_wielder_condition_array);
	auto curCondArgCount = templeFuncs.Obj_Get_IdxField_NumItems(item, obj_f_item_pad_wielder_argument_array);

	// Add the condition name hash to the list
	auto key = templeFuncs.StringHash(cond->condName);
	templeFuncs.Obj_Set_IdxField_byValue(item, obj_f_item_pad_wielder_condition_array, curCondCount, key);

	auto idx = curCondArgCount;
	for (auto arg : args) {
		templeFuncs.Obj_Set_IdxField_byValue(item, obj_f_item_pad_wielder_argument_array, idx, arg);
		idx++;
	}
}

bool ConditionSystem::AddTo(objHndl handle, const CondStruct* cond, const vector<int>& args) {
	assert(args.size() == cond->numArgs);

	auto dispatcher = objects.GetDispatcher(handle);

	if (!dispatcher) {
		return false;
	}

	return _ConditionAddDispatchArgs(dispatcher, &dispatcher->conditions, const_cast<CondStruct*>(cond), args) != 0;
}

bool ConditionSystem::AddTo(objHndl handle, const string& name, const vector<int>& args) {
	auto cond = GetByName(name);
	if (!cond) {
		logger->warn("Unable to find condition {}", name);
		return false;
	}

	return AddTo(handle, cond, args);
}

int32_t ConditionSystem::CondNodeGetArg(CondNode* condNode, uint32_t argIdx)
{
	if (argIdx < condNode->condStruct->numArgs)
	{
		return condNode->args[argIdx];
	}
	return 0;
}

void ConditionSystem::CondNodeSetArg(CondNode* condNode, uint32_t argIdx, uint32_t argVal)
{
	if (argIdx < condNode->condStruct->numArgs)
	{
		condNode->args[argIdx] = argVal;
	}
}

void ConditionSystem::CondNodeAddToSubDispNodeArray(Dispatcher* dispatcher, CondNode* condNodeNew)
{
	_CondNodeAddToSubDispNodeArray(dispatcher, condNodeNew);
}

void ConditionSystem::InitCondFromCondStructAndArgs(Dispatcher* dispatcher, CondStruct* condStruct, int* condargs)
{
	CondNode **v4; 
	SubDispNode *subDispNode; 
	CondNode *condNode; 

	auto *condNodeNew = new CondNode(condStruct);
	v4 = &dispatcher->conditions;
	while (*v4)
	{
		v4 = & (*v4)->nextCondNode;
	}
	*v4 = condNodeNew;

	for (auto i = 0; i < condStruct->numArgs; i++)
	{
		condNodeNew->args[i] = condargs[i];
	}
	conds.CondNodeAddToSubDispNodeArray(dispatcher, condNodeNew);
	for (subDispNode = dispatcher->subDispNodes[dispTypeConditionAddFromD20StatusInit]; subDispNode; subDispNode = subDispNode->next)
	{
		if (subDispNode->subDispDef->dispKey == 0)
		{
			condNode = subDispNode->condNode;
			if (!(condNode->flags & 1) && condNode == condNodeNew)
				subDispNode->subDispDef->dispCallback(subDispNode,dispatcher->objHnd, dispTypeConditionAddFromD20StatusInit, 0, nullptr);
		}
	}
}

void ConditionSystem::SetPermanentModArgsFromDataFields(Dispatcher* dispatcher, CondStruct* condStruct, int* condArgs)
{
	addresses.SetPermanentModArgsFromDataFields(dispatcher, condStruct, condArgs);
}

void ConditionSystem::DispatcherCondsResetFlag2(Dispatcher* dispatcher)
{
	CondNode *condNode; 
	for (condNode = dispatcher->permanentMods; condNode; condNode = condNode->nextCondNode)
		condNode->flags &= 0xFFFFFFFD;
	for (condNode = dispatcher->itemConds; condNode; condNode = condNode->nextCondNode)
		condNode->flags &= 0xFFFFFFFD;
}

int ConditionSystem::GetActiveCondsNum(Dispatcher* dispatcher)
{ 
	int numConds=0; 

	CondNode *cond = dispatcher->conditions;
	while (cond)
	{
		if (!(cond->flags & 1))
			++numConds;
		cond = cond->nextCondNode;
	}
	return numConds;
}

int ConditionSystem::GetPermanentModsAndItemCondCount(Dispatcher* dispatcher)
{
	int numConds = 0;
	CondNode *cond = dispatcher->permanentMods;

	while (cond)
	{
		if (!(cond->flags & 1))
			++numConds;
		cond = cond->nextCondNode;
	}

	cond = dispatcher->itemConds;
	while(cond)
	{
		if (!(cond->flags & 1))
			++numConds;
		cond = cond->nextCondNode;
	}
	return numConds;
}

int ConditionSystem::ConditionsExtractInfo(Dispatcher* dispatcher, int activeCondIdx, int* hashkeyOut, int* condArgsOut)
{
	CondNode *cond;
	int n; 
	int numArgs; 


	cond = dispatcher->conditions;              
	n = 0;
	while (cond)
	{
		if (!(cond->flags & 1))
		{
			if (n == activeCondIdx)
			{
				*hashkeyOut = conds.hashmethods.GetCondStructHashkey(cond->condStruct);
				int numArgs = cond->condStruct->numArgs;
				for (int i = 0; i < numArgs; i++)
				{
					condArgsOut[i] = cond->args[i];
				}
				return cond->condStruct->numArgs;
			}
			++n;
		}
		cond = cond->nextCondNode;
	}
	if (!cond) return 0;

	*hashkeyOut = conds.hashmethods.GetCondStructHashkey(cond->condStruct);
	numArgs = cond->condStruct->numArgs;
	for (int i = 0; i < numArgs; i++)
	{
		condArgsOut[i] = cond->args[i];
	}
	return cond->condStruct->numArgs;
}

int ConditionSystem::PermanentAndItemModsExtractInfo(Dispatcher* dispatcher, int permModIdx, int* hashkeyOut, int* condArgsOut)
{
	
	int i=0; 
	CondNode *cond;

	cond = dispatcher->permanentMods;
	while (cond )
	{
		if (!(cond->flags & 1))
		{
			if (i == permModIdx)
			{
				*hashkeyOut = conds.hashmethods.GetCondStructHashkey(cond->condStruct);
				int numArgs = cond->condStruct->numArgs;
				for (int i = 0; i < numArgs; i++)
				{
					condArgsOut[i] = cond->args[i];
				}
				return cond->condStruct->numArgs;
			}
			++i;
		}
		cond = cond->nextCondNode;
	}


	cond = dispatcher->itemConds;
	while (cond)
	{
		if (! (cond->flags & 1))
		{
			if (i == permModIdx)
			{
				*hashkeyOut = conds.hashmethods.GetCondStructHashkey(cond->condStruct);
				int numArgs = cond->condStruct->numArgs;
				for (i = 0; i < numArgs; i++)
				{
					condArgsOut[i] = cond->args[i];
				}
				return cond->condStruct->numArgs;
			}
			i++;
		}
		cond = cond->nextCondNode;
	}
	if (!cond) return 0;
}

int* ConditionSystem::CondNodeGetArgPtr(CondNode* condNode, int argIdx)
{
	if (argIdx < condNode->condStruct->numArgs)
		return (int*)&condNode->args[argIdx];
	return 0;
}

 int __cdecl AoODisableRadialMenuInit(DispatcherCallbackArgs args)
{
	RadialMenuEntry radEntry;
	radEntry.SetDefaults();
	radEntry.maxArg = 1;
	radEntry.minArg = 0;
	radEntry.type = RadialMenuEntryType::Toggle;
	radEntry.actualArg = (int)conds.CondNodeGetArgPtr(args.subDispNode->condNode, 0);
	radEntry.callback = (void (__cdecl*)(objHndl, RadialMenuEntry*))temple_address(0x100F0200);
	MesLine mesLine;
	mesLine.key = 5105; //disable AoOs
	if (!mesFuncs.GetLine(*combatSys.combatMesfileIdx, &mesLine) )
	{
		sprintf((char*)temple_address(0x10EEE228), "Disable Attacks of Opportunity");
		mesLine.value = (char*) temple_address(0x10EEE228);
	};
	//mesFuncs.GetLine_Safe(*combatSys.combatMesfileIdx, &mesLine);
	radEntry.text = (char*)mesLine.value;
	radEntry.helpId = conds.hashmethods.StringHash("TAG_RADIAL_MENU_DISABLE_AOOS");
	int parentNode = radialMenus.GetStandardNode(RadialMenuStandardNode::Options);
	radialMenus.AddChildNode(args.objHndCaller, &radEntry, parentNode);
	return 0;
}

int __cdecl AoODisableQueryAoOPossible(DispatcherCallbackArgs args)
{
	DispIoD20Query * dispIo = dispatch.DispIoCheckIoType7((DispIoD20Query*)args.dispIO);
	if (dispIo->return_val && conds.CondNodeGetArg(args.subDispNode->condNode, 0))
	{
		dispIo->return_val = 0;
	}
	return 0;
}