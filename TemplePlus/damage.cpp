
#include "stdafx.h"
#include "damage.h"
#include "dice.h"
#include "bonus.h"
#include "ai.h"
#include "gamesystems/objects/objsystem.h"
#include "critter.h"
#include "weapon.h"
#include "combat.h"
#include "history.h"
#include "float_line.h"
#include "sound.h"
#include "anim.h"
#include "ui/ui_logbook.h"

static_assert(temple::validate_size<DispIoDamage, 0x550>::value, "DispIoDamage");

static struct DamageAddresses : temple::AddressTable {

	int (__cdecl *DoDamage)(objHndl target, objHndl src, uint32_t dmgDice, DamageType type, int attackPowerType, signed int reduction, int damageDescMesKey, int actionType);

	int (__cdecl *Heal)(objHndl target, objHndl healer, uint32_t dmgDice, D20ActionType actionType);

	void (__cdecl *HealSpell)(objHndl target, objHndl healer, uint32_t dmgDice, D20ActionType actionType, int spellId);

	int (__cdecl*HealSubdual)(objHndl target, int amount);

	int (__cdecl*DoSpellDamage)(objHndl victim, objHndl attacker, uint32_t dmgDice, DamageType type, int attackPowerType, int reduction, int damageDescMesKey, int actionType, int spellId, int flags);

	bool (__cdecl *SavingThrow)(objHndl obj, objHndl attacker, int dc, SavingThrowType type, int flags);
	
	bool (__cdecl *SavingThrowSpell)(objHndl obj, objHndl attacker, int dc, SavingThrowType type, int flags, int spellId);

	bool (__cdecl *ReflexSaveAndDamage)(objHndl obj, objHndl attacker, int dc, int reduction, int flags, int dicePacked, DamageType damageType, int attackPower, D20ActionType actionType, int spellId);

	void(__cdecl * DamagePacketInit)(DamagePacket *dmgPkt);
	int (__cdecl *AddDamageDice)(DamagePacket *dmgPkt, int dicePacked, DamageType damType, unsigned int damageMesLine);


	int (__cdecl *AddAttackPowerType)(DamagePacket *dmgPkt, int attackPowerType); // allows you to bypass DR with the same attackPowerType bitmask
	int (__cdecl *AddDamageBonus)(DamagePacket *damPkt, int bonValue, int bonType, int bonMesLineNum, char *bonDescr);
	int (__cdecl *AddPhysicalDR)(DamagePacket *damPkt, int DRAmount, int bypasserBitmask, unsigned int damageMesLine); // DR vs. normal physical attack
	int (__cdecl *AddDamageModFactor)(DamagePacket *damage, float dmgFactor, DamageType damType, unsigned int damageMesLine); // use this to grant immunities / vulnerabilities by setting the factor to 0.0 or 2.0
	int (__cdecl *AddIncorporealImmunity)(DamagePacket *dmgPkt);
	int (__cdecl *AddDR)(DamagePacket *damPkt, int DRAmount, DamageType damType, int damageMesLine);
	int (__cdecl *AddDamResistanceWithCause)(DamagePacket *a1, int DRAmount, DamageType damType, unsigned int damageMesLine, const char *causedBy);
	int (__cdecl *EnsureMinimumDamage1)(DamagePacket *dmgPkt); // if overall damage is 0 or less, gives it a bonus so it's exactly 1
	void (__cdecl *SetMaximizedFlag)(DamagePacket *dmgPkt);
	void (__cdecl *SetEmpoweredFlag)(DamagePacket *);
	int (__cdecl *CalcDamageModFromDR)(DamagePacket *damPkt, DamageReduction *damReduc, DamageType attackDamType, DamageType attackDamType2); // sets the damageReduced field in damReduc
	void(__cdecl *CalcDamageModFromFactor)(DamagePacket *damPkt, DamageReduction *damReduc, DamageType attackDamType, DamageType attackDamType2); // same, but for "factor" type DamageReduction (i.e. it doesn't consider damResAmount)
	int (__cdecl *GetDamageTypeOverallDamage)(DamagePacket *damPkt, DamageType damType);
	int(__cdecl *GetOverallDamage)(DamagePacket *damagePacket);
	void(__cdecl *CalcFinalDamage)(DamagePacket *damPkt);
	MesHandle *damageMes;

	DamageAddresses() {

		rebase(damageMes, 0x102E3B30);

		rebase(DoDamage, 0x100B8D70);
		rebase(Heal, 0x100B7DF0);
		rebase(HealSpell, 0x100B81D0);
		rebase(DoSpellDamage, 0x100B7F80);
		rebase(HealSubdual, 0x100B9030);
		rebase(SavingThrow, 0x100B4F20);
		rebase(SavingThrowSpell, 0x100B83C0);
		rebase(ReflexSaveAndDamage, 0x100B9500);

		rebase(DamagePacketInit, 0x100E0390);
		rebase(AddDamageDice, 0x100E03F0);

		rebase(AddAttackPowerType, 0x100E0520);
		rebase(AddDamageBonus, 0x100E05E0);
		rebase(AddPhysicalDR, 0x100E0610);
		rebase(AddDamageModFactor, 0x100E06D0);
		rebase(AddIncorporealImmunity ,0x100E0780);
		rebase(AddDR, 0x100E0830);
		rebase(AddDamResistanceWithCause, 0x100E08F0);
		rebase(EnsureMinimumDamage1, 0x100E09B0);
		rebase(SetMaximizedFlag, 0x100E0A50);
		rebase(SetEmpoweredFlag, 0x100E0A60);
		rebase(CalcDamageModFromDR, 0x100E0C90);
		rebase(CalcDamageModFromFactor, 0x100E0E00);
		rebase(GetDamageTypeOverallDamage, 0X100E1210);
		rebase(GetOverallDamage, 0x100E1360);
		rebase(CalcFinalDamage, 0x100E16F0);

	}
} addresses;

Damage damage;

int DamagePacket::AddEtherealImmunity(){
	if (damModCount >= 5)
		return 0;


	MesLine line;
	line.key = 134;
	mesFuncs.GetLine_Safe(damage.damageMes, &line);
	damageFactorModifiers[damModCount].dmgFactor = 0;
	damageFactorModifiers[damModCount].type = DamageType::Unspecified;
	damageFactorModifiers[damModCount].attackPowerType = 0;
	damageFactorModifiers[damModCount].typeDescription= line.value;
	damageFactorModifiers[damModCount++].causedBy = nullptr;

	return 1;
}

int DamagePacket::AddDamageDice(uint32_t dicePacked, DamageType damType, int damageMesLine, const char* text){
	if (!damage.AddDamageDice(this, dicePacked, damType, damageMesLine))
		return 0;

	if (text){
		dice[diceCount-1].causedBy = text;
	}

	return 1;
}

BOOL DamagePacket::AddDamageBonus(int32_t damBonus, int bonType, int bonMesline, const char* causeDesc){
	bonuses.AddBonusWithDesc(damBonus, bonType, bonMesline, (char*)causeDesc);
	return 1;
}

int DamagePacket::AddPhysicalDR(int amount, int bypasserBitmask, int damageMesLine){
	return damage.AddPhysicalDR(this, amount, bypasserBitmask, (unsigned)damageMesLine);
}

void DamagePacket::AddAttackPower(int attackPower)
{
	this->attackPowerType |= attackPower;
}

void DamagePacket::CalcFinalDamage(){ // todo hook this
	for (auto i=0u; i < this->diceCount; i++){
		auto &dice = this->dice[i];
		if (dice.rolledDamage < 0){
			Dice diceUnpacked;
			diceUnpacked.FromPacked(dice.dicePacked);
			if (this->flags & 1) // maximiuzed
			{
				dice.rolledDamage = diceUnpacked.GetModifier() + diceUnpacked.GetCount() * diceUnpacked.GetSides();
			} else // normal
			{
				dice.rolledDamage = diceUnpacked.Roll();
			}

			if (this->flags & 2) //empowered
			{
				dice.rolledDamage *= 1.5;
			}
		}
	}

	this->finalDamage = temple::GetRef<int(__cdecl)(DamagePacket*, DamageType)>(0x100E1210)(this, DamageType::Unspecified);
}

int DamagePacket::GetOverallDamageByType(DamageType damType)
{
	// TODO
	auto damTot = (double)0.0;

	for (auto i=0u; i<this->diceCount; i++)	{
		
	}

	return damTot;
}

DamagePacket::DamagePacket(){
	diceCount = 0;
	damResCount = 0;
	damModCount = 0;
	attackPowerType = 0;
	finalDamage = 0;
	flags = 0;
	description = nullptr;
	critHitMultiplier = 1;

}

void Damage::DealDamage(objHndl victim, objHndl attacker, const Dice& dice, DamageType type, int attackPower, int reduction, int damageDescId, D20ActionType actionType) {

	addresses.DoDamage(victim, attacker, dice.ToPacked(), type, attackPower, reduction, damageDescId, actionType);

}

void Damage::DealSpellDamage(objHndl tgt, objHndl attacker, const Dice& dice, DamageType type, int attackPower, int reduction, int damageDescId, D20ActionType actionType, int spellId, int flags) {

	SpellPacketBody spPkt(spellId);
	if (!tgt)
		return;

	if (attacker && attacker != tgt && critterSys.AllegianceShared(tgt, attacker))
		floatSys.FloatCombatLine(tgt, 107); // friendly fire

	aiSys.ProvokeHostility(attacker, tgt, 1, 0);

	if (critterSys.IsDeadNullDestroyed(tgt))
		return;

	DispIoDamage evtObjDam;
	evtObjDam.attackPacket.d20ActnType = actionType;
	evtObjDam.attackPacket.attacker = attacker;
	evtObjDam.attackPacket.victim = tgt;
	evtObjDam.attackPacket.dispKey = 1;
	evtObjDam.attackPacket.flags = (D20CAF)(flags | D20CAF_HIT);

	if (attacker && objects.IsCritter(attacker)){
		if (flags & D20CAF_SECONDARY_WEAPON)
			evtObjDam.attackPacket.weaponUsed = inventory.ItemWornAt(attacker, EquipSlot::WeaponSecondary);
		else
			evtObjDam.attackPacket.weaponUsed = inventory.ItemWornAt(attacker, EquipSlot::WeaponPrimary);

		if (evtObjDam.attackPacket.weaponUsed && objects.GetType(evtObjDam.attackPacket.weaponUsed) != obj_t_weapon)
			evtObjDam.attackPacket.weaponUsed = objHndl::null;

		evtObjDam.attackPacket.ammoItem = combatSys.CheckRangedWeaponAmmo(attacker);
	} else
	{
		evtObjDam.attackPacket.weaponUsed = objHndl::null;
		evtObjDam.attackPacket.ammoItem = objHndl::null;
	}

	if (reduction != 100){
		addresses.AddDamageModFactor(&evtObjDam.damage,  reduction * 0.01f, type, damageDescId);
	}

	evtObjDam.damage.AddDamageDice(dice.ToPacked(), type, 103);
	evtObjDam.damage.AddAttackPower(attackPower);
	auto mmData = (MetaMagicData)spPkt.metaMagicData;
	if (mmData.metaMagicEmpowerSpellCount)
		evtObjDam.damage.flags |= 2; // empowered
	if (mmData.metaMagicFlags & 1)
		evtObjDam.damage.flags |= 1; // maximized
	temple::GetRef<int>(0x10BCA8AC) = 0; // is weapon damage

	DamageCritter(attacker, tgt, evtObjDam);

	//addresses.DoSpellDamage(tgt, attacker, dice.ToPacked(), type, attackPower, reduction, damageDescId, actionType, spellId, flags);

}

int Damage::DealAttackDamage(objHndl attacker, objHndl tgt, int d20Data, D20CAF flags, D20ActionType actionType)
{
	aiSys.ProvokeHostility(attacker, tgt, 1, 0);

	auto tgtObj = objSystem->GetObject(tgt);
	if (critterSys.IsDeadNullDestroyed(tgt)){
		return -1;
	}

	DispIoDamage evtObjDam;
	evtObjDam.attackPacket.d20ActnType = actionType;
	evtObjDam.attackPacket.attacker = attacker;
	evtObjDam.attackPacket.victim = tgt;
	evtObjDam.attackPacket.dispKey = d20Data;
	evtObjDam.attackPacket.flags = flags;

	auto &weaponUsed = evtObjDam.attackPacket.weaponUsed;
	if (flags & D20CAF_SECONDARY_WEAPON)
		weaponUsed = inventory.ItemWornAt(attacker, EquipSlot::WeaponSecondary);
	else
		weaponUsed = inventory.ItemWornAt(attacker, EquipSlot::WeaponPrimary);

	if (weaponUsed && objects.GetType(weaponUsed) != obj_t_weapon){
		weaponUsed = objHndl::null;
	}

	evtObjDam.attackPacket.ammoItem = combatSys.CheckRangedWeaponAmmo(attacker);

	if ( flags & D20CAF_CONCEALMENT_MISS){
		histSys.CreateRollHistoryLineFromMesfile(11, attacker, tgt);
		floatSys.FloatCombatLine(attacker, 45); // Miss (Concealment)!
		auto soundId = inventory.GetSoundIdForItemEvent(weaponUsed, attacker, tgt, 6);
		sound.PlaySoundAtObj(soundId, attacker);
		d20Sys.d20SendSignal(attacker, DK_SIG_Attack_Made, (int)&evtObjDam, 0);
		return -1;
	}

	if (!(flags & D20CAF_HIT)) {
		floatSys.FloatCombatLine(attacker, 29);
		d20Sys.d20SendSignal(attacker, DK_SIG_Attack_Made, (int)&evtObjDam, 0);

		auto soundId = inventory.GetSoundIdForItemEvent(weaponUsed, attacker, tgt, 6);
		sound.PlaySoundAtObj(soundId, attacker);

		if (flags & D20CAF_DEFLECT_ARROWS){
			floatSys.FloatCombatLine(tgt, 5052);
			histSys.CreateRollHistoryLineFromMesfile(12, attacker, tgt);
		}

		// dodge animation
		if (!critterSys.IsDeadOrUnconscious(tgt) && !critterSys.IsProne(tgt)){
			animationGoals.PushDodge(attacker, tgt);
		}
		return -1;
	}

	if (tgt && attacker && critterSys.AllegianceShared(tgt, attacker)){ // TODO check that this solves the infamous "Friendly Fire" float for NPCs
		floatSys.FloatCombatLine(tgt, 107); // Friendly Fire
	}

	auto isUnconsciousAlready = critterSys.IsDeadOrUnconscious(tgt);


	dispatch.DispatchDamage(attacker, &evtObjDam, dispTypeDealingDamage, DK_NONE);
	if (evtObjDam.attackPacket.flags & D20CAF_CRITICAL){

		// get extra Hit Dice and apply them
		DispIoAttackBonus evtObjCritDice;
		evtObjCritDice.attackPacket.victim = tgt;
		evtObjCritDice.attackPacket.d20ActnType = evtObjDam.attackPacket.d20ActnType;
		evtObjCritDice.attackPacket.attacker = attacker;
		evtObjCritDice.attackPacket.dispKey = d20Data;
		evtObjCritDice.attackPacket.flags = evtObjDam.attackPacket.flags;
		if (evtObjDam.attackPacket.flags & D20CAF_SECONDARY_WEAPON){
			evtObjCritDice.attackPacket.weaponUsed = inventory.ItemWornAt(attacker, EquipSlot::WeaponSecondary);
		} else
			evtObjCritDice.attackPacket.weaponUsed = inventory.ItemWornAt(attacker, EquipSlot::WeaponPrimary);
		if (evtObjCritDice.attackPacket.weaponUsed && objects.GetType(evtObjCritDice.attackPacket.weaponUsed) != obj_t_weapon)
			evtObjCritDice.attackPacket.weaponUsed = objHndl::null;
		evtObjCritDice.attackPacket.ammoItem = combatSys.CheckRangedWeaponAmmo(attacker);
		auto extraHitDice = dispatch.DispatchAttackBonus(attacker, objHndl::null, &evtObjCritDice, dispTypeGetCriticalHitExtraDice, DK_NONE);
		auto critMultiplierApply = temple::GetRef<BOOL(__cdecl)(DamagePacket&, int, int)>(0x100E1640); // damagepacket, multiplier, damage.mes line
		critMultiplierApply(evtObjDam.damage, extraHitDice + 1, 102);
		floatSys.FloatCombatLine(attacker, 12);
		
		// play sound
		auto soundId = critterSys.SoundmapCritter(tgt, 0);
		sound.PlaySoundAtObj(soundId, tgt);
		soundId = inventory.GetSoundIdForItemEvent(evtObjCritDice.attackPacket.weaponUsed, attacker, tgt, 7);
		sound.PlaySoundAtObj(soundId, attacker);

		// increase crit hits in logbook
		uiLogbook.IncreaseCritHits(attacker);
	} else
	{
		auto soundId = inventory.GetSoundIdForItemEvent(evtObjDam.attackPacket.weaponUsed, attacker, tgt, 5);
		sound.PlaySoundAtObj(soundId, attacker);
	}

	temple::GetRef<int>(0x10BCA8AC) = 1; // physical damage Flag used for logbook recording
	DamageCritter(attacker, tgt, evtObjDam);

	// play damage effect particles
	for (auto i=0; i < evtObjDam.damage.diceCount; i++){
		temple::GetRef<void(__cdecl)(objHndl, DamageType, int)>(0x10016A90)(tgt, evtObjDam.damage.dice[i].type, evtObjDam.damage.dice[i].rolledDamage);
	}


	// signal events
	if (!isUnconsciousAlready && critterSys.IsDeadOrUnconscious(tgt)){
		d20Sys.d20SendSignal(attacker, DK_SIG_Dropped_Enemy, (int)&evtObjDam, 0);
	}

	return addresses.GetDamageTypeOverallDamage(&evtObjDam.damage, DamageType::Unspecified);
}

int Damage::DealWeaponlikeSpellDamage(objHndl tgt, objHndl attacker, const Dice & dice, DamageType type, int attackPower, int damFactor, int damageDescId, D20ActionType actionType, int spellId, D20CAF flags, int prjoectileIdx)
{

	SpellPacketBody spPkt(spellId);
	if (!tgt)
		return -1;

	if (attacker && attacker != tgt && critterSys.AllegianceShared(tgt, attacker))
		floatSys.FloatCombatLine(tgt, 107); // friendly fire

	aiSys.ProvokeHostility(attacker, tgt, 1, 0);

	if (critterSys.IsDeadNullDestroyed(tgt))
		return -1;

	if (combatSys.IsFlankedBy(tgt, attacker))
		*(int*)&flags |= D20CAF_FLANKED;

	DispIoDamage evtObjDam;
	evtObjDam.attackPacket.d20ActnType = actionType;
	evtObjDam.attackPacket.attacker = attacker;
	evtObjDam.attackPacket.victim = tgt;
	evtObjDam.attackPacket.dispKey = prjoectileIdx;
	evtObjDam.attackPacket.flags = (D20CAF)(flags | D20CAF_HIT);

	if (attacker && objects.IsCritter(attacker)) {
		if (flags & D20CAF_SECONDARY_WEAPON)
			evtObjDam.attackPacket.weaponUsed = inventory.ItemWornAt(attacker, EquipSlot::WeaponSecondary);
		else
			evtObjDam.attackPacket.weaponUsed = inventory.ItemWornAt(attacker, EquipSlot::WeaponPrimary);

		if (evtObjDam.attackPacket.weaponUsed && objects.GetType(evtObjDam.attackPacket.weaponUsed) != obj_t_weapon)
			evtObjDam.attackPacket.weaponUsed = objHndl::null;

		evtObjDam.attackPacket.ammoItem = combatSys.CheckRangedWeaponAmmo(attacker);
	}
	else
	{
		evtObjDam.attackPacket.weaponUsed = objHndl::null;
		evtObjDam.attackPacket.ammoItem = objHndl::null;
	}

	if (damFactor != 100) {
		addresses.AddDamageModFactor(&evtObjDam.damage, damFactor * 0.01f, type, damageDescId);
	}
	
	if (flags & D20CAF_CONCEALMENT_MISS) {
		histSys.CreateRollHistoryLineFromMesfile(11, attacker, tgt);
		floatSys.FloatCombatLine(attacker, 45); // Miss (Concealment)!
		// d20Sys.d20SendSignal(attacker, DK_SIG_Attack_Made, (int)&evtObjDam, 0); // casting a spell isn't considered an attack action
		return -1;
	}

	if (!(flags & D20CAF_HIT)) {
		floatSys.FloatCombatLine(attacker, 29);

		// dodge animation
		if (!critterSys.IsDeadOrUnconscious(tgt) && !critterSys.IsProne(tgt)) {
			animationGoals.PushDodge(attacker, tgt);
		}
		return -1;
	}

	temple::GetRef<int>(0x10BCA8AC) = 0; // is weapon damage

	// get damage dice
	evtObjDam.damage.AddDamageDice(dice.ToPacked(), type, 103);
	evtObjDam.damage.AddAttackPower(attackPower);
	auto mmData = (MetaMagicData)spPkt.metaMagicData;
	if (mmData.metaMagicEmpowerSpellCount)
		evtObjDam.damage.flags |= 2; // empowered
	if (mmData.metaMagicFlags & 1)
		evtObjDam.damage.flags |= 1; // maximized

	if (evtObjDam.attackPacket.flags & D20CAF_CRITICAL) {
		auto extraHitDice = 1;
		auto critMultiplierApply = temple::GetRef<BOOL(__cdecl)(DamagePacket&, int, int)>(0x100E1640); // damagepacket, multiplier, damage.mes line
		critMultiplierApply(evtObjDam.damage, extraHitDice + 1, 102);
		floatSys.FloatCombatLine(attacker, 12);

		// play sound
		auto soundId = critterSys.SoundmapCritter(tgt, 0);
		sound.PlaySoundAtObj(soundId, tgt);

		// increase crit hits in logbook
		uiLogbook.IncreaseCritHits(attacker);
	}



	dispatch.DispatchDamage(attacker, &evtObjDam, dispTypeDealingDamageWeaponlikeSpell, DK_NONE);

	


	DamageCritter(attacker, tgt, evtObjDam);

	return -1;
}

void Damage::DamageCritter(objHndl attacker, objHndl tgt, DispIoDamage & evtObjDam){
	temple::GetRef<void(__cdecl)(objHndl, objHndl, DispIoDamage&)>(0x100B6B30)(attacker, tgt, evtObjDam);
}

void Damage::Heal(objHndl target, objHndl healer, const Dice& dice, D20ActionType actionType) {
	addresses.Heal(target, healer, dice.ToPacked(), actionType);
}

void Damage::HealSpell(objHndl target, objHndl healer, const Dice& dice, D20ActionType actionType, int spellId) {
	addresses.HealSpell(target, healer, dice.ToPacked(), actionType, spellId);
}

void Damage::HealSubdual(objHndl target, int amount) {
	addresses.HealSubdual(target, amount);
}

bool Damage::SavingThrow(objHndl obj, objHndl attacker, int dc, SavingThrowType type, int flags) {
	return addresses.SavingThrow(obj, attacker, dc, type, flags);
}

bool Damage::SavingThrowSpell(objHndl obj, objHndl attacker, int dc, SavingThrowType type, int flags, int spellId) {
	return addresses.SavingThrowSpell(obj, attacker, dc, type, flags, spellId);
}

bool Damage::ReflexSaveAndDamage(objHndl obj, objHndl attacker, int dc, int reduction, int flags, const Dice& dice, DamageType damageType, int attackPower, D20ActionType actionType, int spellId) {
	return addresses.ReflexSaveAndDamage(obj, attacker, dc, reduction, flags, dice.ToPacked(), damageType, attackPower, actionType, spellId);
}

void Damage::DamagePacketInit(DamagePacket* dmgPkt)
{
	dmgPkt->diceCount=0;
	dmgPkt->damResCount=0;
	dmgPkt->damModCount=0;
	dmgPkt->attackPowerType=0;
	dmgPkt->finalDamage=0;
	dmgPkt->flags=0;
	dmgPkt->description=0;
	dmgPkt->critHitMultiplier=1;
	bonusSys.initBonusList(&dmgPkt->bonuses);
}

int Damage::AddDamageBonusWithDescr(DamagePacket* damage, int damBonus, int bonType, int bonusMesLine, char* desc)
{
	bonusSys.bonusAddToBonusListWithDescr(&damage->bonuses, damBonus, bonType, bonusMesLine, desc );
	return 1;
}


int Damage::AddPhysicalDR(DamagePacket* damPkt, int DRAmount, int bypasserBitmask, unsigned int damageMesLine)
{
	MesLine mesLine; 

	if (damPkt->damResCount < 5u)
	{
		mesLine.key = damageMesLine;
		mesFuncs.GetLine_Safe(*addresses.damageMes, &mesLine);
		damPkt->damageResistances[damPkt->damResCount].damageReductionAmount = DRAmount;
		damPkt->damageResistances[damPkt->damResCount].dmgFactor = 0;
		damPkt->damageResistances[damPkt->damResCount].type = DamageType::SlashingAndBludgeoningAndPiercing;
		damPkt->damageResistances[damPkt->damResCount].attackPowerType = bypasserBitmask;
		damPkt->damageResistances[damPkt->damResCount].typeDescription = mesLine.value;
		damPkt->damageResistances[damPkt->damResCount++].causedBy = 0;
		return 1;
	}
	return  0;
	
}

const char* Damage::GetMesline(unsigned damageMesLine){
	MesLine mesline(damageMesLine);
	mesFuncs.GetLine_Safe(damageMes, &mesline);
	return mesline.value;
}

int Damage::AddDamageDice(DamagePacket* dmgPkt, int dicePacked, DamageType damType, unsigned int damageMesLine){
	if (dmgPkt->diceCount >= 5)
		return FALSE;

	const char* line = damage.GetMesline(damageMesLine);

	auto _damType = damType;
	if (damType == DamageType::Unspecified)	{
		if (dmgPkt->diceCount > 0)
			_damType = dmgPkt->dice[0].type;
	}
	auto diceCount = dmgPkt->diceCount;
	dmgPkt->dice[dmgPkt->diceCount++] = DamageDice(dicePacked, _damType, line);

	return TRUE;
	//return addresses.AddDamageDice(dmgPkt, dicePacked, damType, damageMesLine);
}

int Damage::AddDamageDiceWithDescr(DamagePacket* dmgPkt, int dicePacked, DamageType damType, unsigned int damageMesLine, char* descr)
{
	if (AddDamageDice(dmgPkt, dicePacked, damType, damageMesLine) == 1)
	{
		dmgPkt->dice[dmgPkt->diceCount - 1].causedBy = descr;
		return 1;
	}
	return 0;
}

Damage::Damage(){
	damageMes = 0;
	// damageMes = addresses.damageMes;
}

void Damage::Init(){
	mesFuncs.Open("tpmes\\damage.mes", &damageMes);
}

void Damage::Exit() const
{
	mesFuncs.Close(damageMes);
}
