/*
**	Command & Conquer(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header:   F:\projects\c&c\vcs\code\house.cpv   2.13   02 Aug 1995 17:03:50   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : HOUSE.CPP                                                    *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : May 21, 1994                                                 *
 *                                                                                             *
 *                  Last Update : August 12, 1995 [JLB]                                        *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   HouseClass::AI -- Process house logic.                                                    *
 *   HouseClass::Abandon_Production -- Abandons production of item type specified.             *
 *   HouseClass::Add_Nuke_Piece -- Add a nuclear piece to the collection.                      *
 *   HouseClass::Adjust_Capacity -- Adjusts the house Tiberium storage capacity.               *
 *   HouseClass::Adjust_Threat -- Adjust threat for the region specified.                      *
 *   HouseClass::As_Pointer -- Converts a house number into a house object pointer.            *
 *   HouseClass::Attacked -- Lets player know if base is under attack.                         *
 *   HouseClass::Available_Money -- Fetches the total credit worth of the house.               *
 *   HouseClass::Begin_Production -- Starts production of the specified object type.           *
 *   HouseClass::Blowup_All -- blows up everything                                             *
 *   HouseClass::Can_Build -- Determines if the aircraft type can be built.                    *
 *   HouseClass::Can_Build -- Determines if the building type can be built.                    *
 *   HouseClass::Can_Build -- Determines if the infantry unit can be built by this house.      *
 *   HouseClass::Can_Build -- Determines if the unit can be built by this house.               *
 *   HouseClass::Can_Build -- General purpose build legality checker.                          *
 *   HouseClass::Clobber_All -- removes house & all its objects                                *
 *   HouseClass::Debug_Dump -- Dumps the house status data to the mono screen.                 *
 *   HouseClass::Detach -- Removes specified object from house tracking systems.               *
 *   HouseClass::Does_Enemy_Building_Exist -- Checks for enemy building of specified type.     *
 *   HouseClass::Flag_Attach -- Attach flag to specified cell (or thereabouts).                *
 *   HouseClass::Flag_Attach -- Attaches the house flag the specified unit.                    *
 *   HouseClass::Flag_Remove -- Removes the flag from the specified target.                    *
 *   HouseClass::Flag_To_Die -- Flags the house to blow up soon.                               *
 *   HouseClass::Flag_To_Lose -- Flags the house to die soon.                                  *
 *   HouseClass::Flag_To_Win -- Flags the house to win soon.                                   *
 *   HouseClass::Harvested -- Adds Tiberium to the harvest storage.                            *
 *   HouseClass::Has_Nuke_Device -- Deteremines if the house has a nuclear device.             *
 *   HouseClass::HouseClass -- Constructor for a house object.                                 *
 *   HouseClass::Init -- init's in preparation for new scenario                                *
 *   HouseClass::Init_Air_Strike -- Add (or reset) the air strike sidebar button.              *
 *   HouseClass::Init_Data -- Initializes the multiplayer color data.                          *
 *   HouseClass::Init_Ion_Cannon -- Initialize the ion cannon countdown.                       *
 *   HouseClass::Init_Nuke_Bomb -- Adds (if necessary) the atom bomb to the sidebar.           *
 *   HouseClass::Is_Ally -- Checks to see if the object is an ally.                            *
 *   HouseClass::Is_Ally -- Determines if the specified house is an ally.                      *
 *   HouseClass::Is_Ally -- Determines if the specified house is an ally.                      *
 *   HouseClass::MPlayer_Defeated -- multiplayer; house is defeated                            *
 *   HouseClass::Make_Air_Strike_Available -- Make the airstrike available.                    *
 *   HouseClass::Make_Ally -- Make the specified house an ally.                                *
 *   HouseClass::Make_Enemy -- Make an enemy of the house specified.                           *
 *   HouseClass::Manual_Place -- Inform display system of building placement mode.             *
 *   HouseClass::One_Time -- Handles one time initialization of the house array.               *
 *   HouseClass::Place_Object -- Places the object (building) at location specified.           *
 *   HouseClass::Place_Special_Blast -- Place a special blast effect at location specified.    *
 *   HouseClass::Power_Fraction -- Fetches the current power output rating.                    *
 *   HouseClass::Read_INI -- Reads house specific data from INI.                               *
 *   HouseClass::Refund_Money -- Refunds money to back to the house.                           *
 *   HouseClass::Remap_Table -- Fetches the remap table for this house object.                 *
 *   HouseClass::Remove_Air_Strike -- Removes the air strike button from the sidebar.          *
 *   HouseClass::Remove_Ion_Cannon -- Disables the ion cannon.                                 *
 *   HouseClass::Remove_Nuke_Bomb -- Removes the nuclear bomb from the sidebar.                *
 *   HouseClass::Sell_Wall -- Tries to sell the wall at the specified location.                *
 *   HouseClass::Silo_Redraw_Check -- Flags silos to be redrawn if necessary.                  *
 *   HouseClass::Special_Weapon_AI -- Fires special weapon.                                    *
 *   HouseClass::Spend_Money -- Removes money from the house.                                  *
 *   HouseClass::Suggest_New_Object -- Determine what would the next buildable object be.      *
 *   HouseClass::Suggested_New_Team -- Determine what team should be created.                  *
 *   HouseClass::Suspend_Production -- Temporarily puts production on hold.                    *
 *   HouseClass::Validate -- validates house pointer														  *
 *   HouseClass::Write_INI -- Writes house specific data into INI file.                        *
 *   HouseClass::delete -- Deallocator function for a house object.                            *
 *   HouseClass::new -- Allocator for a house class.                                           *
 *   HouseClass::operator HousesType -- Conversion to HousesType operator.                     *
 *   HouseClass::~HouseClass -- Default destructor for a house object.                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"function.h"

enum {
	SKIRMISH_WAVE_ASSEMBLE = 0,
	SKIRMISH_WAVE_LAUNCH = 1,
	SKIRMISH_WAVE_COOLDOWN = 2,
	SKIRMISH_WAVE_REGROUP = 3
};


static int Skirmish_AI_Index(HouseClass const * house)
{
	if (!house) {
		return(-1);
	}

	int index = (int)house->Class->House - (int)HOUSE_MULTI1;

	if (index >= 0 && index < MAX_PLAYERS) {
		return(index);
	}

	return(-1);
}


static int Skirmish_AI_Map_Size_Class(void)
{
	int map_cells = Map.MapCellWidth * Map.MapCellHeight;

	if (map_cells <= 2304) {
		return(-1);
	}
	if (map_cells >= 4096) {
		return(1);
	}
	return(0);
}


static int Skirmish_AI_Wave_State(HouseClass const * house)
{
	int index = Skirmish_AI_Index(house);

	if (index == -1) {
		return(SKIRMISH_WAVE_ASSEMBLE);
	}

	return(Bound(MPlayerAIWaveState[index], SKIRMISH_WAVE_ASSEMBLE, SKIRMISH_WAVE_REGROUP));
}


static void Skirmish_AI_Set_Wave_State(HouseClass const * house, int state)
{
	int index = Skirmish_AI_Index(house);

	if (index != -1) {
		MPlayerAIWaveState[index] = Bound(state, SKIRMISH_WAVE_ASSEMBLE, SKIRMISH_WAVE_REGROUP);
	}
}


static int Skirmish_AI_Wave_Target(int skill, int personality, int map_class)
{
	int needed;

	switch (skill) {
		default:
		case 0:
			needed = Random_Pick(3, 5);
			break;

		case 1:
			needed = Random_Pick(4, 6);
			break;

		case 2:
			needed = Random_Pick(5, 7);
			break;
	}

	if (map_class < 0) {
		needed = MAX(2, needed - 1);
	} else if (map_class > 0) {
		needed = MIN(7, needed + 1);
	}

	if (personality == 0) {
		needed = MAX(2, needed - 1);
	} else if (personality == 1) {
		needed = MIN(8, needed + 1);
	}

	return(needed);
}


static int Skirmish_AI_Relaunch_Target(int skill, int personality, int map_class)
{
	int needed;

	switch (skill) {
		default:
		case 0:
			needed = 3;
			break;

		case 1:
			needed = 4;
			break;

		case 2:
			needed = 5;
			break;
	}

	if (map_class < 0) {
		needed = MAX(2, needed - 1);
	} else if (map_class > 0) {
		needed = MIN(6, needed + 1);
	}

	if (personality == 0) {
		needed = MAX(2, needed - 1);
	} else if (personality == 1) {
		needed = MIN(7, needed + 1);
	}

	return(needed);
}


static long Skirmish_AI_Wave_Prep_Delay(int personality, int map_class, bool regroup)
{
	if (personality == 0) {
		if (map_class < 0) {
			return(TICKS_PER_SECOND * Random_Pick(regroup ? 6 : 4, regroup ? 12 : 10));
		}
		if (map_class > 0) {
			return(TICKS_PER_SECOND * Random_Pick(regroup ? 10 : 8, regroup ? 16 : 14));
		}
		return(TICKS_PER_SECOND * Random_Pick(regroup ? 8 : 6, regroup ? 14 : 12));
	}

	if (personality == 1) {
		if (map_class < 0) {
			return(TICKS_PER_SECOND * Random_Pick(regroup ? 10 : 8, regroup ? 18 : 16));
		}
		if (map_class > 0) {
			return(TICKS_PER_SECOND * Random_Pick(regroup ? 14 : 12, regroup ? 24 : 24));
		}
		return(TICKS_PER_SECOND * Random_Pick(regroup ? 12 : 10, regroup ? 22 : 20));
	}

	if (map_class < 0) {
		return(TICKS_PER_SECOND * Random_Pick(regroup ? 8 : 6, regroup ? 14 : 12));
	}
	if (map_class > 0) {
		return(TICKS_PER_SECOND * Random_Pick(regroup ? 12 : 10, regroup ? 20 : 18));
	}
	return(TICKS_PER_SECOND * Random_Pick(regroup ? 10 : 8, regroup ? 18 : 16));
}


static long Skirmish_AI_Wave_Assemble_Delay(int skill, int personality, int map_class)
{
	switch (skill) {
		default:
		case 0:
			if (personality == 0) {
				if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(2, 4));
				if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(4, 6));
				return(TICKS_PER_MINUTE * Random_Pick(3, 5));
			}
			if (personality == 1) {
				if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(4, 6));
				if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(6, 8));
				return(TICKS_PER_MINUTE * Random_Pick(5, 7));
			}
			if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(3, 5));
			if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(5, 7));
			return(TICKS_PER_MINUTE * Random_Pick(4, 6));

		case 1:
			if (personality == 0) {
				if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(1, 2));
				if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(2, 4));
				return(TICKS_PER_MINUTE * Random_Pick(1, 3));
			}
			if (personality == 1) {
				if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(2, 4));
				if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(4, 6));
				return(TICKS_PER_MINUTE * Random_Pick(3, 5));
			}
			if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(1, 3));
			if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(3, 5));
			return(TICKS_PER_MINUTE * Random_Pick(2, 4));

		case 2:
			if (personality == 0) {
				if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(1, 1));
				if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(1, 3));
				return(TICKS_PER_MINUTE * Random_Pick(1, 2));
			}
			if (personality == 1) {
				if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(2, 3));
				if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(3, 5));
				return(TICKS_PER_MINUTE * Random_Pick(2, 4));
			}
			if (map_class < 0) return(TICKS_PER_MINUTE * Random_Pick(1, 2));
			if (map_class > 0) return(TICKS_PER_MINUTE * Random_Pick(2, 4));
			return(TICKS_PER_MINUTE * Random_Pick(1, 3));
	}
}


static long Skirmish_AI_Wave_Cooldown_Delay(int personality, int map_class, bool active_wave)
{
	if (active_wave) {
		if (map_class < 0) {
			return(TICKS_PER_SECOND * Random_Pick(12, 24));
		}
		if (map_class > 0) {
			return(TICKS_PER_SECOND * Random_Pick(24, 45));
		}
		return(TICKS_PER_SECOND * Random_Pick(20, 40));
	}

	if (personality == 0) {
		if (map_class < 0) return(TICKS_PER_SECOND * Random_Pick(10, 16));
		if (map_class > 0) return(TICKS_PER_SECOND * Random_Pick(16, 24));
		return(TICKS_PER_SECOND * Random_Pick(12, 20));
	}
	if (personality == 1) {
		if (map_class < 0) return(TICKS_PER_SECOND * Random_Pick(14, 22));
		if (map_class > 0) return(TICKS_PER_SECOND * Random_Pick(22, 32));
		return(TICKS_PER_SECOND * Random_Pick(18, 28));
	}
	if (map_class < 0) return(TICKS_PER_SECOND * Random_Pick(12, 18));
	if (map_class > 0) return(TICKS_PER_SECOND * Random_Pick(18, 28));
	return(TICKS_PER_SECOND * Random_Pick(15, 24));
}


static int Skirmish_AI_Active_Wave_Count(HouseClass const * house, int * units = NULL)
{
	int count = 0;
	int total = 0;

	for (int index = 0; index < Teams.Count(); index++) {
		TeamClass * team = Teams.Ptr(index);

		if (!team || team->House != house || !team->Total || !team->IsHasBeen || team->IsUnderStrength || team->IsReforming) {
			continue;
		}

		count++;
		total += team->Total;
	}

	if (units) {
		*units = total;
	}

	return(count);
}


static int Skirmish_AI_Regrouping_Units(HouseClass const * house, int * teams_out = NULL)
{
	int total = 0;
	int teams = 0;

	for (int index = 0; index < Teams.Count(); index++) {
		TeamClass * team = Teams.Ptr(index);

		if (!team || team->House != house || !team->Total || !team->IsHasBeen) {
			continue;
		}
		if (!team->IsReforming && !team->IsUnderStrength) {
			continue;
		}

		total += team->Total;
		teams++;
	}

	if (teams_out) {
		*teams_out = teams;
	}

	return(total);
}


static bool Skirmish_AI_Force_Regrouped_Wave(HouseClass const * house, int needed)
{
	TeamClass * best = NULL;
	int best_total = 0;

	for (int index = 0; index < Teams.Count(); index++) {
		TeamClass * team = Teams.Ptr(index);

		if (!team || team->House != house || !team->Total || !team->IsHasBeen) {
			continue;
		}
		if (!team->IsReforming && !team->IsUnderStrength) {
			continue;
		}

		if (team->Total > best_total) {
			best = team;
			best_total = team->Total;
		}
	}

	if (best && best_total >= needed) {
		best->Force_Active();
		return(true);
	}

	return(false);
}


static int Skirmish_AI_Available_Combat_Count(HouseClass const * house)
{
	int combat = 0;

	for (int index = 0; index < Units.Count(); index++) {
		UnitClass * unit = Units.Ptr(index);

		if (unit && !unit->IsInLimbo && unit->House == house && unit->Strength > 0 && !unit->Team &&
			*unit != UNIT_HARVESTER && *unit != UNIT_MCV) {
			combat++;
		}
	}

	for (int index = 0; index < Infantry.Count(); index++) {
		InfantryClass * infantry = Infantry.Ptr(index);

		if (infantry && !infantry->IsInLimbo && infantry->House == house && infantry->Strength > 0 && !infantry->Team &&
			*infantry != INFANTRY_E7) {
			combat++;
		}
	}

	return(combat);
}


static bool Skirmish_AI_Is_Recovery_Mode(long flags, int power, int drain, int refinery_count, int combat_units)
{
	if (!(flags & STRUCTF_POWER) || !(flags & STRUCTF_REFINERY)) {
		return(true);
	}

	if (power <= drain || (power - drain) < 25) {
		return(true);
	}

	if (!(flags & (STRUCTF_WEAP | STRUCTF_AIRSTRIP)) && combat_units >= 3) {
		return(true);
	}

	if (!(flags & STRUCTF_RADAR) && combat_units >= 5) {
		return(true);
	}

	if (refinery_count == 0) {
		return(true);
	}

	return(false);
}


static bool Skirmish_AI_Economy_Harassed(long flags, bool under_pressure, int refinery_count, int harvester_count, int combat_units)
{
	if (!(flags & STRUCTF_REFINERY)) {
		return(true);
	}

	if (refinery_count <= 0) {
		return(true);
	}

	if (refinery_count == 1 && harvester_count == 0) {
		return(true);
	}

	if (under_pressure && refinery_count <= 1 && harvester_count <= 1 && combat_units >= 3) {
		return(true);
	}

	return(false);
}


static bool Skirmish_AI_Core_Reserve(StructType reserve_build)
{
	switch (reserve_build) {
		case STRUCT_POWER:
		case STRUCT_REFINERY:
		case STRUCT_BARRACKS:
		case STRUCT_HAND:
		case STRUCT_WEAP:
		case STRUCT_AIRSTRIP:
			return(true);

		default:
			break;
	}

	return(false);
}


static int Skirmish_AI_Min_Combat_Force(int skill, int personality)
{
	int minimum = 4;

	switch (skill) {
		default:
		case 0:
			minimum = 3;
			break;

		case 1:
			minimum = 4;
			break;

		case 2:
			minimum = 5;
			break;
	}

	if (personality == 0) {
		minimum++;
	} else if (personality == 1) {
		minimum = MAX(3, minimum - 1);
	}

	return(minimum);
}


static int Skirmish_AI_Personality(HouseClass const * house)
{
	int index = Skirmish_AI_Index(house);

	if (index != -1) {
		return(Bound(MPlayerAIPersonality[index], 0, 2));
	}
	return(1);
}


static void Skirmish_AI_Enemy_Mix(HouseClass const * house, int & enemy_infantry, int & enemy_units, int & enemy_buildings)
{
	enemy_infantry = 0;
	enemy_units = 0;
	enemy_buildings = 0;

	for (int index = 0; index < Infantry.Count(); index++) {
		InfantryClass * infantry = Infantry.Ptr(index);

		if (infantry && !infantry->IsInLimbo && infantry->Strength > 0 && !house->Is_Ally(infantry) &&
			*infantry != INFANTRY_E7) {
			enemy_infantry++;
		}
	}

	for (int index = 0; index < Units.Count(); index++) {
		UnitClass * unit = Units.Ptr(index);

		if (unit && !unit->IsInLimbo && unit->Strength > 0 && !house->Is_Ally(unit) &&
			*unit != UNIT_HARVESTER && *unit != UNIT_MCV) {
			enemy_units++;
		}
	}

	for (int index = 0; index < Buildings.Count(); index++) {
		BuildingClass * building = Buildings.Ptr(index);

		if (building && !building->IsInLimbo && building->Strength > 0 && !house->Is_Ally(building)) {
			enemy_buildings++;
		}
	}
}


static bool House_Has_Live_Assets(HouseClass const * house)
{
	for (int i = 0; i < ::Aircraft.Count(); i++) {
		AircraftClass * craft = ::Aircraft.Ptr(i);

		if (craft && !craft->IsInLimbo && craft->House == house && craft->Strength > 0) {
			return(true);
		}
	}

	for (int i = 0; i < ::Units.Count(); i++) {
		UnitClass * unit = ::Units.Ptr(i);

		if (unit && !unit->IsInLimbo && unit->House == house && unit->Strength > 0) {
			return(true);
		}
	}

	for (int i = 0; i < ::Infantry.Count(); i++) {
		InfantryClass * infantry = ::Infantry.Ptr(i);

		if (infantry && !infantry->IsInLimbo && infantry->House == house && infantry->Strength > 0) {
			return(true);
		}
	}

	for (int i = 0; i < ::Buildings.Count(); i++) {
		BuildingClass * building = ::Buildings.Ptr(i);

		if (building && !building->IsInLimbo && building->House == house && building->Strength > 0) {
			return(true);
		}
	}

	return(false);
}


/***********************************************************************************************
 * HouseClass::Validate -- validates house pointer															  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *		none.																												  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *		1 = ok, 0 = error																								  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *		none.																												  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/09/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
#ifdef CHEAT_KEYS
int HouseClass::Validate(void) const
{
	int num;

	num = Houses.ID(this);
	if (num < 0 || num >= HOUSE_MAX) {
		Validate_Error("HOUSE");
		return (0);
	}
	else
		return (1);
}
#else
#define	Validate()
#endif


/***********************************************************************************************
 * HouseClass::operator HousesType -- Conversion to HousesType operator.                       *
 *                                                                                             *
 *    This operator will automatically convert from a houses class object into the HousesType  *
 *    enumerated value.                                                                        *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Returns with the object's HousesType value.                                        *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
HouseClass::operator HousesType(void) const
{
	Validate();
	return(Class->House);
}


/***********************************************************************************************
 * HouseClass::As_Pointer -- Converts a house number into a house object pointer.              *
 *                                                                                             *
 *    Use this routine to convert a house number into the house pointer that it represents.    *
 *    A simple index into the Houses template array is not sufficient, since the array order   *
 *    is arbitrary. An actual scan through the house object is required in order to find the   *
 *    house object desired.                                                                    *
 *                                                                                             *
 * INPUT:   house -- The house type number to look up.                                         *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the house object that the house number represents.       *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
HouseClass * HouseClass::As_Pointer(HousesType house)
{
	for (int index = 0; index < Houses.Count(); index++) {
		if (Houses.Ptr(index)->Class->House == house) {
			return(Houses.Ptr(index));
		}
	}
	return(0);
}


/***********************************************************************************************
 * HouseClass::One_Time -- Handles one time initialization of the house array.                 *
 *                                                                                             *
 *    This basically calls the constructor for each of the houses in the game. All other       *
 *    data specific to the house is initialized when the scenario is loaded.                   *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Only call this ONCE at the beginning of the game.                               *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/09/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::One_Time(void)
{
//	for (HousesType index = HOUSE_FIRST; index < HOUSE_COUNT; index++) {
//		new(index) HouseClass;
//	}
}


#ifdef CHEAT_KEYS
/***********************************************************************************************
 * HouseClass::Debug_Dump -- Dumps the house status data to the mono screen.                   *
 *                                                                                             *
 *    This utility function will output the current status of the house class to the mono      *
 *    screen. Through this information bugs may be fixed or detected.                          *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/31/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Debug_Dump(MonoClass *) const
{
	Validate();
}
#endif


/***********************************************************************************************
 * HouseClass::new -- Allocator for a house class.                                             *
 *                                                                                             *
 *    This is the allocator for a house class. Since there can be only                         *
 *    one of each type of house, this is allocator has restricted                              *
 *    functionality. Any attempt to allocate a house structure for a                           *
 *    house that already exists, just returns a pointer to the previously                      *
 *    allocated house.                                                                         *
 *                                                                                             *
 * INPUT:   house -- The house to allocate a class object for.                                 *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the allocated class object.                              *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/22/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void * HouseClass::operator new(size_t) noexcept
{
	void * ptr = Houses.Allocate();
	if (ptr) {
		((HouseClass *)ptr)->IsActive = true;
	}
	return(ptr);
}


/***********************************************************************************************
 * HouseClass::delete -- Deallocator function for a house object.                              *
 *                                                                                             *
 *    This function marks the house object as "deallocated". Such a                            *
 *    house object is available for reallocation later.                                        *
 *                                                                                             *
 * INPUT:   ptr   -- Pointer to the house object to deallocate.                                *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/22/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::operator delete(void *ptr)
{
	if (ptr) {
		((HouseClass *)ptr)->IsActive = false;
	}
	Houses.Free((HouseClass *)ptr);
}


/***********************************************************************************************
 * HouseClass::HouseClass -- Constructor for a house object.                                   *
 *                                                                                             *
 *    This function is the constructor and it marks the house object                           *
 *    as being allocated.                                                                      *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/22/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
HouseClass::HouseClass(HousesType house) :
	Class(&HouseTypeClass::As_Reference(house)),
	IonCannon(ION_CANNON_GONE_TIME, VOX_ION_READY, VOX_ION_CHARGING, VOX_ION_CHARGING, VOX_NO_POWER),
	AirStrike(AIR_CANNON_GONE_TIME, VOX_AIRSTRIKE_READY, VOX_NONE, VOX_NOT_READY, VOX_NOT_READY),
	NukeStrike(NUKE_GONE_TIME, VOX_NUKE_AVAILABLE, VOX_NONE, VOX_NOT_READY, VOX_NO_POWER)
{

	for (HousesType i = HOUSE_FIRST; i < HOUSE_COUNT; i++) {
		UnitsKilled[i] = 0;
		BuildingsKilled[i] = 0;
	}
	WhoLastHurtMe = house;			// init this to myself

	IsVisionary = false;
	IsFreeHarvester = false;
	Blockage = 0;
	UnitsLost = 0;
	BuildingsLost = 0;

	NewActiveBScan = 0;
	ActiveBScan = 0;
	NewActiveUScan = 0;
	ActiveUScan = 0;
	NewActiveIScan = 0;
	ActiveIScan = 0;
	NewActiveAScan = 0;
	ActiveAScan = 0;

	strcpy((char *)Name, "Computer");	// Default computer name.
	JustBuilt = STRUCT_NONE;
	AlertTime = 0;
	IsAlerted = false;
	IsAirstrikePending = false;
	AircraftFactory = -1;
	AircraftFactories = 0;
	ActLike = Class->House;
	Allies = 0;
	AScan = 0;
	NukeDest = 0;
	BlitzTime.Clear();
	BScan = 0;
	BuildingFactories = 0;
	BuildingFactory = -1;
	Capacity = 0;
	Credits = 0;
	CreditsSpent = 0;
	CurBuildings = 0;
	CurUnits = 0;
	DamageTime = DAMAGE_DELAY;
	Drain = 0;
	Edge = SOURCE_NORTH;
	FlagHome = 0;
	FlagLocation = TARGET_NONE;
	HarvestedCredits = 0;
	HouseTriggers[house].Clear();
	IGaveUp = false;
	InfantryFactories = 0;
	InfantryFactory = -1;
	InitialCredits = 0;
	InitialCredits = 0;
	IScan = 0;
	IsRecalcNeeded = true;
	IsCivEvacuated = false;
	IsDefeated = false;
	IsDiscovered = false;
	IsHuman = false;
	IsMaxedOut = false;
	IsStarted = false;
	IsToDie = false;
	IsToLose = false;
	IsToWin = false;
	Make_Ally(house);
	MaxBuilding = 0;
	MaxUnit = 0;
	NewAScan = 0;
	NewBScan = 0;
	NewIScan = 0;
	NewUScan = 0;
	NukePieces = 0x07;
	Power = 0;
	RemapTable = Class->RemapTable;
	RemapColor = Class->RemapColor;
	Resigned = false;
	SpeakAttackDelay = 1;
	SpeakMaxedDelay = 1;
	SpeakMoneyDelay = 1;
	SpeakPowerDelay = 1;
	SpecialFactories = 0;
	SpecialFactory = -1;
	TeamTime = TEAM_DELAY;
	Tiberium = 0;
	TriggerTime = 0;
	UnitFactories = 0;
	UnitFactory = -1;
	UScan = 0;
	memset((void *)&Regions[0], 0x00, sizeof(Regions));

	AircraftTotals = new UnitTrackerClass( (int) AIRCRAFT_COUNT);
	InfantryTotals = new UnitTrackerClass( (int) INFANTRY_COUNT);
	UnitTotals = new UnitTrackerClass ( (int) UNIT_COUNT);
	BuildingTotals = new UnitTrackerClass ( (int) STRUCT_COUNT);

	DestroyedAircraft = new UnitTrackerClass ( (int) AIRCRAFT_COUNT);
	DestroyedInfantry = new UnitTrackerClass( (int) INFANTRY_COUNT);
	DestroyedUnits = new UnitTrackerClass ( (int) UNIT_COUNT);
	DestroyedBuildings = new UnitTrackerClass ( (int) STRUCT_COUNT);

	CapturedBuildings = new UnitTrackerClass ( (int) STRUCT_COUNT);
	TotalCrates = new UnitTrackerClass ( TOTAL_CRATE_TYPES );	//15 crate types
}


HouseClass::~HouseClass (void)
{
	delete AircraftTotals;
	delete InfantryTotals;
	delete UnitTotals;
	delete BuildingTotals;

	delete DestroyedAircraft;
	delete DestroyedInfantry;
	delete DestroyedUnits;
	delete DestroyedBuildings;

	delete CapturedBuildings;
	delete TotalCrates;
}

/***********************************************************************************************
 * HouseClass::Can_Build -- General purpose build legality checker.                            *
 *                                                                                             *
 *    This routine is called when it needs to be determined if the specified object type can   *
 *    be built by this house. Production and sidebar maintenance use this routine heavily.     *
 *                                                                                             *
 * INPUT:   type  -- Pointer to the type of object that legality is to be checked for.         *
 *                                                                                             *
 *          house -- This is the house to check for legality against. Note that this might     *
 *                   not be 'this' house since the check could be from a captured factory.     *
 *                   Captured factories build what the original owner of them could build.     *
 *                                                                                             *
 * OUTPUT:  Can the specified object be built?                                                 *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/04/1995 JLB : Created.                                                                 *
 *   08/12/1995 JLB : Updated for GDI building sandbag walls in #9.                            *
 *=============================================================================================*/
bool HouseClass::Can_Build(TechnoTypeClass const * type, HousesType house) const
{
	Validate();
	if (!type || !type->IsBuildable || !((1L << house) & type->Ownable)) return(false);

	/*
	**	The computer can always build everthing.
	*/
	if (!IsHuman && GameToPlay != GAME_SKIRMISH) return(true);

	/*
	**	Perform some equivalency fixups for the building existance flags.
	*/
	long flags = ActiveBScan;
	int pre = type->Pre;
	if (flags & STRUCTF_ADVANCED_POWER) flags |= STRUCTF_POWER;
	if (flags & STRUCTF_HAND) flags |= STRUCTF_BARRACKS;
	if (flags & STRUCTF_OBELISK) flags |= STRUCTF_ATOWER;
	if (flags & STRUCTF_TEMPLE) flags |= STRUCTF_EYE;
	if (flags & STRUCTF_AIRSTRIP) flags |= STRUCTF_WEAP;
	if (flags & STRUCTF_SAM) flags |= STRUCTF_HELIPAD;

	/*
	**	Multiplayer game uses a different legality check for building.
	*/
	if (GameToPlay != GAME_NORMAL || (Special.IsJurassic && AreThingiesEnabled)) {
		return((pre & flags) == pre && type->Level <= BuildLevel);
	}

#ifdef NEWMENU
	int level = BuildLevel;
#else
	int level = Scenario;
#endif

	/*
	**	Special check to make the mission objective buildings the prerequisite
	**	for the stealth tank in mission #11 only.
	*/
	if (house == HOUSE_BAD &&
		type->What_Am_I() == RTTI_UNITTYPE &&
		((UnitTypeClass const *)type)->Type == UNIT_STANK &&
		level == 11) {

		pre = STRUCTF_MISSION;
		level = type->Scenario;
	}

	/*
	**	Special case check to ensure that GDI doesn't get the bazooka guy
	**	until mission #8.
	*/
	if (house == HOUSE_GOOD &&
		type->What_Am_I() == RTTI_INFANTRYTYPE &&
		((InfantryTypeClass const *)type)->Type == INFANTRY_E3 &&
		level < 7) {

		return(false);
	}

	/*
	**	Special check to allow GDI to build the MSAM by mission #9
	**	and no sooner.
	*/
	if (house == HOUSE_GOOD &&
		type->What_Am_I() == RTTI_UNITTYPE &&
		((UnitTypeClass const *)type)->Type == UNIT_MLRS &&
		level < 9) {

		return(false);
	}

	/*
	**	Special case to disable the APC from the Nod player.
	*/
	if (house == HOUSE_BAD &&
		type->What_Am_I() == RTTI_UNITTYPE &&
		((UnitTypeClass const *)type)->Type == UNIT_APC) {

		return(false);
	}

	/*
	**	Ensure that the Temple of Nod cannot be built by GDI even
	**	if GDI has captured the Nod construction yard.
	*/
	if (type->What_Am_I() == RTTI_BUILDINGTYPE &&
		(((BuildingTypeClass const *)type)->Type == STRUCT_TEMPLE || ((BuildingTypeClass const *)type)->Type == STRUCT_OBELISK) &&
		Class->House == HOUSE_GOOD) {

		return(false);
	}

	/*
	**	Ensure that the rocket launcher tank cannot be built by Nod.
	*/
	if (type->What_Am_I() == RTTI_UNITTYPE &&
		((UnitTypeClass const *)type)->Type == UNIT_MLRS &&
		Class->House == HOUSE_BAD) {

		return(false);
	}

	/*
	**	Ensure that the ion cannon cannot be built if
	**	Nod has captured the GDI construction yard.
	*/
	if (type->What_Am_I() == RTTI_BUILDINGTYPE &&
		(((BuildingTypeClass const *)type)->Type == STRUCT_EYE) &&
		Class->House == HOUSE_BAD) {

		return(false);
	}

	/*
	**	Nod can build the advanced power plant at scenario #12.
	*/
	if (house == HOUSE_BAD &&
		level >= 12 &&
		type->What_Am_I() == RTTI_BUILDINGTYPE &&
		((BuildingTypeClass const *)type)->Type == STRUCT_ADVANCED_POWER) {

		level = type->Scenario;
	}

	/*
	**	Nod cannot build a helipad in the normal game.
	*/
	if (house == HOUSE_BAD &&
		type->What_Am_I() == RTTI_BUILDINGTYPE &&
		((BuildingTypeClass const *)type)->Type == STRUCT_HELIPAD) {

		return(false);
	}

	/*
	**	GDI can build the sandbag wall only from scenario #9 onwards.
	*/
	if (house == HOUSE_GOOD &&
		level < 8 &&
		type->What_Am_I() == RTTI_BUILDINGTYPE &&
		((BuildingTypeClass const *)type)->Type == STRUCT_SANDBAG_WALL) {

		return(false);
	}

	/*
	**	GDI has a special second training mission. Adjust the scenario level so that
	**	scenario two will still feel like scenario #1.
	*/
	if (house == HOUSE_GOOD && level == 2) {
		level = 1;
	}

	if (Debug_Cheat) level = 98;
	return((pre & flags) == pre && type->Scenario <= level);
}


/***********************************************************************************************
 * HouseClass::Can_Build -- Determines if the building type can be built.                      *
 *                                                                                             *
 *    This routine is used by the construction preparation code to building a list of building *
 *    types that can be built. It determines if a building can be built by checking if the     *
 *    prerequisite buildings have been built (and still exist) as well as checking to see if   *
 *    the house can build the specified structure.                                             *
 *                                                                                             *
 * INPUT:   s     -- The structure type number that is being checked.                          *
 *                                                                                             *
 *          house -- The house number to use when determining if the object can be built.      *
 *                   This is necessary because the current owner of the factory doesn't        *
 *                   control what the factory can produce. Rather, the original builder of     *
 *                   the factory controls this.                                                *
 *                                                                                             *
 * OUTPUT:  bool; Can this structure type be built at this time?                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/08/1994 JLB : Created.                                                                 *
 *   05/31/1995 JLB : Handles specified ownership override.                                    *
 *=============================================================================================*/
bool HouseClass::Can_Build(StructType s, HousesType house) const
{
	Validate();
	return(Can_Build(&BuildingTypeClass::As_Reference(s), house));
}


/***********************************************************************************************
 * HouseClass::Can_Build -- Determines if the infantry unit can be built by this house.        *
 *                                                                                             *
 *    Use this routine to determine if the infantry type specified can be built by this        *
 *    house. It determines this by checking the ownership allowed bits in the infantry         *
 *    type class.                                                                              *
 *                                                                                             *
 * INPUT:   infantry -- The infantry type to check against this house.                         *
 *                                                                                             *
 *          house -- The house number to use when determining if the object can be built.      *
 *                   This is necessary because the current owner of the factory doesn't        *
 *                   control what the factory can produce. Rather, the original builder of     *
 *                   the factory controls this.                                                *
 *                                                                                             *
 * OUTPUT:  bool; Can the infantry be produced by this house?                                  *
 *                                                                                             *
 * WARNINGS:   It does not check to see if there is a functional barracks available, but       *
 *             merely checks to see if it is legal for this house to build that infantry       *
 *             type.                                                                           *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/09/1994 JLB : Created.                                                                 *
 *   05/31/1995 JLB : Handles specified ownership override.                                    *
 *=============================================================================================*/
bool HouseClass::Can_Build(InfantryType infantry, HousesType house) const
{
	Validate();
	return(Can_Build(&InfantryTypeClass::As_Reference(infantry), house));
}


/***********************************************************************************************
 * HouseClass::Can_Build -- Determines if the unit can be built by this house.                 *
 *                                                                                             *
 *    This routine is used to determine if the unit type specified can in fact be built by     *
 *    this house. It checks the ownable bits in the unit's type to determine this.             *
 *                                                                                             *
 * INPUT:   unit  -- The unit type to check against this house.                                *
 *                                                                                             *
 *          house -- The house number to use when determining if the object can be built.      *
 *                   This is necessary because the current owner of the factory doesn't        *
 *                   control what the factory can produce. Rather, the original builder of     *
 *                   the factory controls this.                                                *
 *                                                                                             *
 * OUTPUT:  bool; Can the unit be built by this house?                                         *
 *                                                                                             *
 * WARNINGS:   This doesn't check to see if there is a functional factory that can build       *
 *             this unit, but merely if the unit can be built according to ownership rules.    *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/09/1994 JLB : Created.                                                                 *
 *   05/31/1995 JLB : Handles specified ownership override.                                    *
 *=============================================================================================*/
bool HouseClass::Can_Build(UnitType unit, HousesType house) const
{
	Validate();
	return(Can_Build(&UnitTypeClass::As_Reference(unit), house));
}


/***********************************************************************************************
 * HouseClass::Can_Build -- Determines if the aircraft type can be built.                      *
 *                                                                                             *
 *    Use this routine to determine if the specified aircraft type can be built. This routine  *
 *    is used by the sidebar and factory to determine what can be built.                       *
 *                                                                                             *
 * INPUT:   aircraft -- The aircraft type to check for build legality.                         *
 *                                                                                             *
 *          house    -- The house that is performing the check. This is typically the house    *
 *                      of the original building of the factory rather than the current        *
 *                      owner.                                                                 *
 *                                                                                             *
 * OUTPUT:  Can this aircraft type be built by the house specified?                            *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/24/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Can_Build(AircraftType aircraft, HousesType house) const
{
	Validate();
	return(Can_Build(&AircraftTypeClass::As_Reference(aircraft), house));
}


/***************************************************************************
 * HouseClass::Init -- init's in preparation for new scenario              *
 *                                                                         *
 * INPUT:                                                                  *
 *      none.                                                              *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      none.                                                              *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   12/07/1994 BR : Created.                                              *
 *   12/17/1994 JLB : Resets tracker bits.                                 *
 *=========================================================================*/
void HouseClass::Init(void)
{
	Houses.Free_All();

	for (HousesType index = HOUSE_FIRST; index < HOUSE_COUNT; index++) {
		HouseTriggers[index].Clear();
	}
}


/***********************************************************************************************
 * HouseClass::AI -- Process house logic.                                                      *
 *                                                                                             *
 *    This handles the AI for the house object. It should be called once per house per game    *
 *    tick. It processes all house global tasks such as low power damage accumulation and      *
 *    house specific trigger events.                                                           *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/27/1994 JLB : Created.                                                                 *
 *   07/17/1995 JLB : Limits EVA speaking unless the player can do something.                  *
 *=============================================================================================*/
void HouseClass::AI(void)
{
	Validate();

	/*
	**	Reset the scan accumulation bits for the next logic pass.
	*/
	IScan = NewIScan;
	BScan = NewBScan;
	UScan = NewUScan;
	AScan = NewAScan;
	ActiveIScan = NewActiveIScan;
	ActiveBScan = NewActiveBScan;
	ActiveUScan = NewActiveUScan;
	ActiveAScan = NewActiveAScan;
	NewIScan = 0;
	NewBScan = 0;
	NewUScan = 0;
	NewAScan = 0;
	NewActiveIScan = 0;
	NewActiveBScan = 0;
	NewActiveUScan = 0;
	NewActiveAScan = 0;

	/*
	**	Check to see if the house wins.
	*/
	if (GameToPlay == GAME_NORMAL && IsToWin && BorrowedTime.Expired() && Blockage <= 0) {
		IsToWin = false;
		if (this == PlayerPtr) {
			PlayerWins = true;
		} else {
			PlayerLoses = true;
		}
	}

	/*
	**	Check to see if the house loses.
	*/
	if (GameToPlay == GAME_NORMAL && IsToLose && BorrowedTime.Expired()) {
		IsToLose = false;
		if (this == PlayerPtr) {
			PlayerLoses = true;
		} else {
			PlayerWins = true;
		}
	}

	/*
	**	Check to see if all objects of this house should be blown up.
	*/
	if (IsToDie && BorrowedTime.Expired()) {
		IsToDie = false;
		Blowup_All();
	}

	/*
	**	Double check power values to correct illegal conditions. It is possible to
	**	get a power output of negative (one usually) as a result of damage sustained
	**	and the fixed point fractional math involved with power adjustements. If the
	**	power rating drops below zero, then make it zero.
	*/
	if (GameToPlay == GAME_NORMAL) {
		Power = MAX(Power, 0);
		Drain = MAX(Drain, 0);
	}

	if (GameToPlay == GAME_SKIRMISH && !IsHuman && IsAlerted && !AlertTime.Expired()) {
		int active_wave_units = 0;
		int active_waves = Skirmish_AI_Active_Wave_Count(this, &active_wave_units);
		int available = Skirmish_AI_Available_Combat_Count(this);
		int regrouping = Skirmish_AI_Regrouping_Units(this);
		int personality = Skirmish_AI_Personality(this);
		int wave_state = Skirmish_AI_Wave_State(this);
		int map_class = Skirmish_AI_Map_Size_Class();
		int needed = Skirmish_AI_Wave_Target(MPlayerAISkill, personality, map_class);
		int relaunch_needed = Skirmish_AI_Relaunch_Target(MPlayerAISkill, personality, map_class);
		int refinery_count = 0;
		int harvester_count = 0;
		bool economy_harassed = false;

		for (int index = 0; index < Buildings.Count(); index++) {
			BuildingClass * building = Buildings.Ptr(index);

			if (building && !building->IsInLimbo && building->House == this && building->Strength > 0 &&
				*building == STRUCT_REFINERY) {
				refinery_count++;
			}
		}
		for (int index = 0; index < Units.Count(); index++) {
			UnitClass * unit = Units.Ptr(index);

			if (unit && !unit->IsInLimbo && unit->House == this && unit->Strength > 0 &&
				*unit == UNIT_HARVESTER) {
				harvester_count++;
			}
		}
		economy_harassed = Skirmish_AI_Economy_Harassed(ActiveBScan, (WhoLastHurtMe != HOUSE_NONE && WhoLastHurtMe != Class->House),
			refinery_count, harvester_count, available + regrouping);

		if (!economy_harassed && !active_waves && active_wave_units < 3) {
			if ((wave_state == SKIRMISH_WAVE_ASSEMBLE || wave_state == SKIRMISH_WAVE_COOLDOWN) && available >= needed) {
				Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_LAUNCH);
				AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, false);
			} else if ((wave_state == SKIRMISH_WAVE_ASSEMBLE || wave_state == SKIRMISH_WAVE_COOLDOWN) && regrouping >= relaunch_needed) {
				Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_REGROUP);
				AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, true);
			} else if (wave_state == SKIRMISH_WAVE_COOLDOWN && (available + regrouping) >= relaunch_needed) {
				Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_REGROUP);
				AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, true);
			}
		}
	}

	/*
	**	If the base has been alerted to the enemy and should be attacking, then
	**	see if the attack timer has expired. If it has, then create the attack
	**	teams.
	*/
	if (IsAlerted && AlertTime.Expired()) {

		/*
		**	Adjusted to reduce maximum number of teams created.
		*/
		int maxteams = Random_Pick(2, (int)(((BuildLevel-1)/3)+1));
		if (GameToPlay == GAME_SKIRMISH && !IsHuman) {
			int combat = 0;
			int active_wave_units = 0;
			int active_waves = Skirmish_AI_Active_Wave_Count(this, &active_wave_units);
			int regrouping = Skirmish_AI_Regrouping_Units(this);
			int personality = Skirmish_AI_Personality(this);
			int wave_state = Skirmish_AI_Wave_State(this);
			int map_class = Skirmish_AI_Map_Size_Class();
			int needed = Skirmish_AI_Wave_Target(MPlayerAISkill, personality, map_class);
			int relaunch_needed = Skirmish_AI_Relaunch_Target(MPlayerAISkill, personality, map_class);
			int refinery_count = 0;
			int harvester_count = 0;
			bool economy_harassed = false;
			maxteams = 1;

			combat = Skirmish_AI_Available_Combat_Count(this);
			for (int index = 0; index < Buildings.Count(); index++) {
				BuildingClass * building = Buildings.Ptr(index);

				if (building && !building->IsInLimbo && building->House == this && building->Strength > 0 &&
					*building == STRUCT_REFINERY) {
					refinery_count++;
				}
			}
			for (int index = 0; index < Units.Count(); index++) {
				UnitClass * unit = Units.Ptr(index);

				if (unit && !unit->IsInLimbo && unit->House == this && unit->Strength > 0 &&
					*unit == UNIT_HARVESTER) {
					harvester_count++;
				}
			}
			economy_harassed = Skirmish_AI_Economy_Harassed(ActiveBScan, (WhoLastHurtMe != HOUSE_NONE && WhoLastHurtMe != Class->House),
				refinery_count, harvester_count, combat + regrouping);

			if (economy_harassed) {
				Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_ASSEMBLE);
				AlertTime = TICKS_PER_SECOND * Random_Pick(25, 45);
			} else if (active_waves || active_wave_units >= 3) {
				Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_COOLDOWN);
				AlertTime = Skirmish_AI_Wave_Cooldown_Delay(personality, map_class, true);
			} else {
				switch (wave_state) {
					default:
					case SKIRMISH_WAVE_ASSEMBLE:
						if (regrouping >= relaunch_needed) {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_REGROUP);
							AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, true);
						} else if (combat >= needed) {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_LAUNCH);
							AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, false);
						} else {
							AlertTime = Skirmish_AI_Wave_Assemble_Delay(MPlayerAISkill, personality, map_class);
						}
						break;

					case SKIRMISH_WAVE_LAUNCH:
						if (combat >= needed) {
							for (int index = 0; index < maxteams; index++) {
								TeamTypeClass const * ttype = Suggested_New_Team(true);
								if (ttype) {
									ScenarioInit++;
									ttype->Create_One_Of();
									ScenarioInit--;
								}
							}
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_COOLDOWN);
							AlertTime = Skirmish_AI_Wave_Cooldown_Delay(personality, map_class, false);
						} else if (regrouping >= relaunch_needed) {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_REGROUP);
							AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, true);
						} else {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_ASSEMBLE);
							AlertTime = Skirmish_AI_Wave_Assemble_Delay(MPlayerAISkill, personality, map_class);
						}
						break;

					case SKIRMISH_WAVE_REGROUP:
						if (regrouping >= relaunch_needed && Skirmish_AI_Force_Regrouped_Wave(this, relaunch_needed)) {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_COOLDOWN);
							AlertTime = Skirmish_AI_Wave_Cooldown_Delay(personality, map_class, false);
						} else if (combat >= needed) {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_LAUNCH);
							AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, false);
						} else {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_ASSEMBLE);
							AlertTime = Skirmish_AI_Wave_Assemble_Delay(MPlayerAISkill, personality, map_class);
						}
						break;

					case SKIRMISH_WAVE_COOLDOWN:
						if (regrouping >= relaunch_needed) {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_REGROUP);
							AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, true);
						} else if (combat >= needed) {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_LAUNCH);
							AlertTime = Skirmish_AI_Wave_Prep_Delay(personality, map_class, false);
						} else {
							Skirmish_AI_Set_Wave_State(this, SKIRMISH_WAVE_ASSEMBLE);
							AlertTime = Skirmish_AI_Wave_Assemble_Delay(MPlayerAISkill, personality, map_class);
						}
						break;
				}
			}
			goto skip_alert_team_timer;
		}
		for (int index = 0; index < maxteams; index++) {
			TeamTypeClass const * ttype = Suggested_New_Team(true);
			if (ttype) {
				ScenarioInit++;
				ttype->Create_One_Of();
				ScenarioInit--;
			}
		}
		if (Special.IsDifficult) {
			AlertTime = (TICKS_PER_MINUTE * Random_Pick(4, 10));
		} else {
			if (Special.IsEasy) {
				AlertTime = (TICKS_PER_MINUTE * Random_Pick(16, 40));
			} else {
				AlertTime = (TICKS_PER_MINUTE * Random_Pick(5, 20));
			}
		}
	}
skip_alert_team_timer:

	/*
	**	Create teams for this house if necessary.
	** (Use the same timer for some extra capture-the-flag logic.)
	*/
	if (TeamTime.Expired()) {
		if (!(GameToPlay == GAME_SKIRMISH && !IsHuman)) {
			TeamTypeClass const * ttype = Suggested_New_Team(false);
			if (ttype) {
				ttype->Create_One_Of();
			}
		}

		/*
		** Also use this timer to detect if someone is sitting on my flag cell.
		*/
		if (Special.IsCaptureTheFlag && GameToPlay != GAME_NORMAL) {
			TechnoClass *techno;
			int damage;
			int count;
			int moving;

			/*
			**	If this house's flag waypoint is a valid cell, see if there's
			**	someone sitting on it.  If so, make the scatter.  If they refuse,
			**	blow them up.
			*/
			if (FlagHome) {
				techno = Map[FlagHome].Cell_Techno();
				if (techno) {
					moving = false;
					techno->Scatter(0,true);

					/*
					**	If the techno doesn't have a valid NavCom, he's not moving,
					**	so blow him up.
					*/
					if (techno->What_Am_I() == RTTI_INFANTRY ||
						techno->What_Am_I() == RTTI_UNIT) {
						if (Target_Legal(((FootClass *)techno)->NavCom)) {
							moving = true;
						}
					}

					/*
					**	If the techno wasn't an infantry or unit (ie he's a building),
					**	or he refuses to move, blow him up
					*/
					if (!moving) {
						count = 0;
						while (!(techno->IsInLimbo) && count++ < 5) {
							damage = 0x7fff;
							Explosion_Damage(techno->Center_Coord(), damage, NULL, WARHEAD_HE);
						}
					}
				}
			}
		}

		/*
		** Randomly create a Visceroid or other disastrous multiplayer object.
		** Create the object, and use Scan_Place_Object to place the object near
		** the center of the map.
		*/
		if (GameToPlay != GAME_NORMAL && Class->House==HOUSE_JP) {
			int rlimit;

			if (Special.IsJurassic && AreThingiesEnabled) {
				rlimit = 450;
			} else {
				rlimit = 1000;
			}

			if (IRandom(0, rlimit) == 0) {
				UnitClass *obj = NULL;
				CELL cell;

				if (Special.IsJurassic && AreThingiesEnabled) {
					obj = new UnitClass(Random_Pick(UNIT_TRIC, UNIT_STEG), HOUSE_JP);
				} else {
					if (BuildLevel >= 7) {
						if (!(UScan & UNITF_VICE)) {
							obj = new UnitClass(UNIT_VICE, HOUSE_JP);
						}
					}
				}

				if (obj) {
					cell = XY_Cell (Map.MapCellX + Random_Pick(0, Map.MapCellWidth - 1),
						Map.MapCellY + Random_Pick(0, Map.MapCellHeight - 1));
					if (!Scan_Place_Object(obj, cell)) {
						delete obj;
					}
				}
			}
		}

		TeamTime.Set(TEAM_DELAY);
	}

	/*
	**	If there is insufficient power, then all buildings that are above
	**	half strength take a little bit of damage.
	*/
	if (DamageTime.Expired()) {

/*
**	No free harvesters for computer or player. - 8/16/95
*/
#ifdef OBSOLETE
		/*
		**	Replace the last harvester if there is a refinery present.
		*/
		if (GameToPlay == GAME_NORMAL &&
			Frame > 5 &&
			(!IsHuman && BuildLevel <= 6) &&
			(ActiveBScan & STRUCTF_REFINERY) != 0 &&
			(UScan & UNITF_HARVESTER) == 0 &&
			!IsFreeHarvester) {

			IsFreeHarvester = true;
			FreeHarvester = TICKS_PER_MINUTE * 2;
		}
#endif

		/*
		**	If a free harvester is to be created and the time is right, then create
		**	the harvester and clear the free harvester pending flag.
		*/
		if (IsFreeHarvester && FreeHarvester.Expired()) {
			IsFreeHarvester = false;
			Create_Special_Reinforcement(this, (TechnoTypeClass *)&UnitTypeClass::As_Reference(UNIT_HARVESTER), NULL);
		}

		/*
		**	When the power is below required, then the buildings will take damage over
		**	time.
		*/
		if (Power_Fraction() < 0x100) {
			for (int index = 0; index < Buildings.Count(); index++) {
				BuildingClass & b = *Buildings.Ptr(index);

				if (b.House == this && b.Health_Ratio() > 0x080) {
					int damage = 1;
					b.Take_Damage(damage, 0, WARHEAD_AP, 0);
				}
			}
		}
		DamageTime.Set(DAMAGE_DELAY);
	}

	/*
	**	If there are no more buildings to sell, then automatically cancel the
	**	sell mode.
	*/
	if (PlayerPtr == this && !BScan && Map.IsSellMode) {
		Map.Sell_Mode_Control(0);
	}

	/*
	**	Various base conditions may be announced to the player.
	*/
	if (PlayerPtr == this) {

		if (SpeakMaxedDelay.Expired() && IsMaxedOut) {
			IsMaxedOut = false;
			if ((Capacity - Tiberium) < 300 && Capacity > 500 && (BScan & (STRUCTF_REFINERY | STRUCTF_CONST))) {
 				Speak(VOX_NEED_MO_CAPACITY);
				SpeakMaxedDelay.Set(Options.Normalize_Delay(SPEAK_DELAY));
			}
		}
		if (SpeakPowerDelay.Expired() && Power_Fraction() < 0x0100) {
			if (BScan & STRUCTF_CONST) {
				Speak(VOX_LOW_POWER);
				SpeakPowerDelay.Set(Options.Normalize_Delay(SPEAK_DELAY));
			}
		}
	}

	/*
	**	If there is a flag associated with this house, then mark it to be
	**	redrawn.
	*/
	if (Target_Legal(FlagLocation)) {
		UnitClass * unit = As_Unit(FlagLocation);
		if (unit) {
			unit->Mark(MARK_CHANGE);
		} else {
			CELL cell = As_Cell(FlagLocation);
			Map[cell].Redraw_Objects();
		}
	}

	bool is_time = false;

	/*
	**	Triggers are only checked every so often. If the trigger timer has expired,
	**	then set the trigger processing flag.
	*/
	if (TriggerTime.Expired()) {
		is_time = true;
		TriggerTime = TICKS_PER_MINUTE/10;
	}

	/*
	**	Check to see if the ion cannon should be removed from the sidebar
	**	because of outside circumstances. The advanced communications facility
	**	being destroyed is a good example of this.
	*/
	if (IonCannon.Is_Present()) {
		if (!(ActiveBScan & STRUCTF_EYE) && !IonCannon.Is_One_Time()) {

			/*
			**	Remove the ion cannon when there is no advanced communication facility.
			**	Note that this will not remove the one time created ion cannon.
			*/
			if (IonCannon.Remove()) {
				if (this == PlayerPtr) Map.Column[1].Flag_To_Redraw();
				IsRecalcNeeded = true;
			}
		} else {

			/*
			**	Turn the ion cannon suspension on or off depending on the available
			**	power. Note that one time ion cannon will not be affected by this.
			*/
			IonCannon.Suspend(Power_Fraction() < 0x0100);

			/*
			**	Process the ion cannon AI and if something changed that would affect
			**	the sidebar, then flag the sidebar to be redrawn.
			*/
			if (IonCannon.AI(this == PlayerPtr)) {
				if (this == PlayerPtr) Map.Column[1].Flag_To_Redraw();
			}
		}

		/*
		**	The computer may decide to fire the ion cannon if it is ready.
		*/
		if (IonCannon.Is_Ready() && !IsHuman) {
			Special_Weapon_AI(SPC_ION_CANNON);
		}

	} else {

		/*
		**	If there is no ion cannon present, but there is an advanced communcation
		**	center available, then make the ion cannon available as well.
		*/
		if ((ActiveBScan & STRUCTF_EYE) &&
			(ActLike == HOUSE_GOOD || GameToPlay != GAME_NORMAL) &&
			(IsHuman || GameToPlay != GAME_NORMAL)) {

			IonCannon.Enable(false, this == PlayerPtr, Power_Fraction() < 0x0100);

			/*
			**	Flag the sidebar to be redrawn if necessary.
			*/
			if (this == PlayerPtr) {
				Map.Add(RTTI_SPECIAL, SPC_ION_CANNON);
				Map.Column[1].Flag_To_Redraw();
			}
		}
	}

	/*
	**	Check to see if the nuke strike should be removed from the sidebar
	**	because of outside circumstances. The Temple of Nod
	**	being destroyed is a good example of this.
	*/
	if (NukeStrike.Is_Present()) {
		if (!(ActiveBScan & STRUCTF_TEMPLE) && (!NukeStrike.Is_One_Time() || GameToPlay == GAME_NORMAL)) {

			/*
			**	Remove the nuke strike when there is no Temple of Nod.
			**	Note that this will not remove the one time created nuke strike.
			*/
			if (NukeStrike.Remove(true)) {
				IsRecalcNeeded = true;
				if (this == PlayerPtr) Map.Column[1].Flag_To_Redraw();
			}
		} else {

			/*
			**	Turn the nuke strike suspension on or off depending on the available
			**	power. Note that one time nuke strike will not be affected by this.
			*/
			NukeStrike.Suspend(Power_Fraction() < 0x0100);

			/*
			**	Process the nuke strike AI and if something changed that would affect
			**	the sidebar, then flag the sidebar to be redrawn.
			*/
			if (NukeStrike.AI(this == PlayerPtr)) {
				if (this == PlayerPtr) Map.Column[1].Flag_To_Redraw();
			}
		}

		/*
		**	The computer may decide to fire the nuclear missile if it is ready.
		*/
		if (NukeStrike.Is_Ready() && !IsHuman) {
			Special_Weapon_AI(SPC_NUCLEAR_BOMB);
		}

	} else {

		/*
		**	If there is no nuke strike present, but there is a Temple of Nod
		**	available, then make the nuke strike strike available.
		*/
		if ((ActiveBScan & STRUCTF_TEMPLE) && Has_Nuke_Device() && (IsHuman || GameToPlay != GAME_NORMAL)) {
			NukeStrike.Enable((GameToPlay == GAME_NORMAL), this == PlayerPtr);

			/*
			**	Flag the sidebar to be redrawn if necessary.
			*/
			if (this == PlayerPtr) {
				Map.Add(RTTI_SPECIAL, SPC_NUCLEAR_BOMB);
				Map.Column[1].Flag_To_Redraw();
			}
		}
	}

	/*
	**	Process the airstrike AI and if something changed that would affect
	**	the sidebar, then flag the sidebar to be redrawn.
	*/
	if (AirStrike.Is_Present()) {
		if (AirStrike.AI(this == PlayerPtr)) {
			if (this == PlayerPtr) Map.Column[1].Flag_To_Redraw();
		}

		/*
		**	The computer may decide to call in the airstrike if it is ready.
		*/
		if (AirStrike.Is_Ready() && !IsHuman) {
			Special_Weapon_AI(SPC_AIR_STRIKE);
		}
	}

	/*
	**	Add the airstrike option if it is pending.
	*/
	if (IsAirstrikePending) {
		IsAirstrikePending = false;
		if (AirStrike.Enable(false, this == PlayerPtr)) {
			AirStrike.Forced_Charge(this == PlayerPtr);
			if (this == PlayerPtr) {
				Map.Add(RTTI_SPECIAL, SPC_AIR_STRIKE);
				Map.Column[1].Flag_To_Redraw();
			}
		}
	}

#ifdef NEVER
	/*
	** The following logic deals with the nuclear warhead state machine.  It
	** handles all the different stages of the temple firing and the rocket
	** travelling up and down.  The rocket explosion is handled by the anim
	** which is attached to the bullet.
	*/
	if (!IsHuman && NukePresent) {
		Special_Weapon_AI(SPC_NUCLEAR_BOMB);

	}
#endif

	/*
	** Special win/lose check for multiplayer games; by-passes the
	** trigger system.  We must wait for non-zero frame, because init
	** may not properly set IScan etc for each house; you have to go
	** through each object's AI before it will be properly set.
	*/
	if (GameToPlay != GAME_NORMAL && !IsDefeated && Frame > 0) {
		bool defeated = (!ActiveBScan && !ActiveAScan && !UScan && !ActiveIScan);

		if (GameToPlay == GAME_SKIRMISH) {
			defeated = !House_Has_Live_Assets(this);
		}

		if (defeated) {
		MPlayer_Defeated();
		}
	}

	for (int index = 0; index < HouseTriggers[Class->House].Count(); index++) {
		TriggerClass * t = HouseTriggers[Class->House][index];

		/*
		**	Check for just built the building trigger event.
		*/
		if (JustBuilt != STRUCT_NONE) {
			if (t->Spring(EVENT_BUILD, Class->House, JustBuilt)) {
				JustBuilt = STRUCT_NONE;
				continue;
			}
		}

		/*
		**	Check for civilian evacuation trigger event.
		*/
		if (IsCivEvacuated && t->Spring(EVENT_EVAC_CIVILIAN, Class->House)) {
			continue;
		}

		/*
		**	Number of buildings destroyed checker.
		*/
		if (t->Spring(EVENT_NBUILDINGS_DESTROYED, Class->House, BuildingsLost)) {
			continue;
		}

		/*
		**	Number of units destroyed checker.
		*/
		if (t->Spring(EVENT_NUNITS_DESTROYED, Class->House, UnitsLost)) {
			continue;
		}

		/*
		**	House has been revealed trigger event.
		*/
		if (IsDiscovered && t->Spring(EVENT_HOUSE_DISCOVERED, Class->House)) {
			IsDiscovered = false;
			continue;
		}

		/*
		**	The "all destroyed" triggers are only processed after a certain
		**	amount of safety time has expired.
		*/
		if (!EndCountDown) {

			/*
			**	All buildings destroyed checker.
			*/
			if (!ActiveBScan) {
				if (t->Spring(EVENT_BUILDINGS_DESTROYED, Class->House)) {
					continue;
				}
			}

			/*
			**	All units destroyed checker.
			*/
			if (!((ActiveUScan & ~(UNITF_GUNBOAT)) | IScan | (ActiveAScan & ~(AIRCRAFTF_TRANSPORT|AIRCRAFTF_CARGO|AIRCRAFTF_A10)))) {
				if (t->Spring(EVENT_UNITS_DESTROYED, Class->House)) {
					continue;
				}
			}

			/*
			**	All buildings AND units destroyed checker.
			*/
			if (!(ActiveBScan | (ActiveUScan & ~(UNITF_GUNBOAT)) | IScan | (ActiveAScan & ~(AIRCRAFTF_TRANSPORT|AIRCRAFTF_CARGO|AIRCRAFTF_A10)))) {
				if (t->Spring(EVENT_ALL_DESTROYED, Class->House)) {
					continue;
				}
			}
		}

		/*
		**	Credit check.
		*/
		if (t->Spring(EVENT_CREDITS, Class->House, Credits)) {
			continue;
		}

		/*
		**	Timeout check.
		*/
		if (is_time && t->Spring(EVENT_TIME, Class->House)) {
			continue;
		}

		/*
		**	All factories destroyed check.
		*/
		if (!(BScan & (STRUCTF_AIRSTRIP|STRUCTF_HAND|STRUCTF_WEAP|STRUCTF_BARRACKS)) && t->Spring(EVENT_NOFACTORIES, Class->House)) {
			continue;
		}
	}

	/*
	**	If a radar facility is not present, but the radar is active, then turn the radar off.
	**	The radar also is turned off when the power gets below 100% capacity.
	*/
	if (PlayerPtr == this) {
		if (Map.Is_Radar_Active()) {
			if (ActiveBScan & (STRUCTF_RADAR|STRUCTF_EYE)) {
				if (Power_Fraction() < 0x0100) {
					Map.Radar_Activate(0);
				}
			} else {
				Map.Radar_Activate(0);
			}
		} else {
			if (ActiveBScan & (STRUCTF_RADAR|STRUCTF_EYE)) {
				if (Power_Fraction() >= 0x0100) {
					Map.Radar_Activate(1);
				}
			} else {
				if (Map.Is_Radar_Existing()) {
					Map.Radar_Activate(4);
				}
			}
		}
	}

	/*
	**	If the production possibilities need to be recalculated, then do so now. This must
	**	occur after the scan bits have been properly updated.
	*/
	if (PlayerPtr == this && IsRecalcNeeded) {
		IsRecalcNeeded = false;
		Map.Recalc();

#ifdef NEVER
		/*
		**	Remove the ion cannon if necessary.
		*/
		if (IonCannon.Is_Present() && !(BScan & STRUCTF_EYE)) {
			IonCannon.Remove();
		}

		/*
		**	Remove the nuclear bomb if necessary.
		*/
		if (NukeStrike.Is_Present() && !(BScan & STRUCTF_TEMPLE)) {
			NukeStrike.Remove();
		}
#endif

		/*
		**	This placement might affect any prerequisite requirements for construction
		**	lists. Update the buildable options accordingly.
		*/
		for (int index = 0; index < Buildings.Count(); index++) {
			BuildingClass * building = Buildings.Ptr(index);
			if (building && building->Owner() == Class->House) {

				building->Update_Specials();
				if (PlayerPtr == building->House) {
					building->Update_Buildables();
				}
			}
		}
	}
}


/***********************************************************************************************
 * HouseClass::Attacked -- Lets player know if base is under attack.                           *
 *                                                                                             *
 *    Call this function whenever a building is attacked (with malice). This function will     *
 *    then announce to the player that his base is under attack. It checks to make sure that   *
 *    this is referring to the player's house rather than the enemy's.                         *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/27/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Attacked(void)
{
	Validate();
	if (SpeakAttackDelay.Expired() && PlayerPtr->Class->House == Class->House) {
		Speak(VOX_BASE_UNDER_ATTACK);
		SpeakAttackDelay.Set(Options.Normalize_Delay(SPEAK_DELAY));

		/*
		**	If there is a trigger event associated with being attacked, process it
		**	now.
		*/
		for (int index = 0; index < HouseTriggers[Class->House].Count(); index++) {
			HouseTriggers[Class->House][index]->Spring(EVENT_ATTACKED, Class->House);
		}
	}
}


/***********************************************************************************************
 * HouseClass::Harvested -- Adds Tiberium to the harvest storage.                              *
 *                                                                                             *
 *    Use this routine whenever Tiberium is harvested. The Tiberium is stored equally between  *
 *    all storage capable buildings for the house. Harvested Tiberium adds to the credit       *
 *    value of the house, but only up to the maximum storage capacity that the house can       *
 *    currently maintain.                                                                      *
 *                                                                                             *
 * INPUT:   tiberium -- The number of Tiberium credits to add to the House's total.            *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/25/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Harvested(unsigned tiberium)
{
	Validate();
	long oldtib = Tiberium;

	Tiberium += tiberium;
	if (Tiberium > Capacity) {
		Tiberium = Capacity;
		IsMaxedOut = true;
	}
	HarvestedCredits += tiberium;
	Silo_Redraw_Check(oldtib, Capacity);
}


/***********************************************************************************************
 * HouseClass::Available_Money -- Fetches the total credit worth of the house.                 *
 *                                                                                             *
 *    Use this routine to determine the total credit value of the house. This is the sum of   *
 *    the harvested Tiberium in storage and the initial unspent cash reserves.                 *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Returns with the total credit value of the house.                                  *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/25/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
long HouseClass::Available_Money(void) const
{
	Validate();
	return(Tiberium + Credits);
}


/***********************************************************************************************
 * HouseClass::Spend_Money -- Removes money from the house.                                    *
 *                                                                                             *
 *    Use this routine to extract money from the house. Typically, this is a result of         *
 *    production spending. The money is extracted from available cash reserves first. When     *
 *    cash reserves are exhausted, then Tiberium is consumed.                                  *
 *                                                                                             *
 * INPUT:   money -- The amount of money to spend.                                             *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/25/1995 JLB : Created.                                                                 *
 *   06/20/1995 JLB : Spends Tiberium before spending cash.                                    *
 *=============================================================================================*/
void HouseClass::Spend_Money(unsigned money)
{
	Validate();
	long oldtib = Tiberium;
	if (money > Tiberium) {
		money -= (unsigned)Tiberium;
		Tiberium = 0;
		Credits -= money;
	} else {
		Tiberium -= money;
	}
	Silo_Redraw_Check(oldtib, Capacity);
	CreditsSpent += money;
}


/***********************************************************************************************
 * HouseClass::Refund_Money -- Refunds money to back to the house.                             *
 *                                                                                             *
 *    Use this routine when money needs to be refunded back to the house. This can occur when  *
 *    construction is aborted. At this point, the exact breakdown of Tiberium or initial       *
 *    credits used for the orignal purchase is lost. Presume as much of the money is in the    *
 *    form of Tiberium as storage capacity will allow.                                         *
 *                                                                                             *
 * INPUT:   money -- The number of credits to refund back to the house.                        *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/25/1995 JLB : Created.                                                                 *
 *   06/01/1995 JLB : Refunded money is never lost                                             *
 *=============================================================================================*/
void HouseClass::Refund_Money(unsigned money)
{
	Validate();
	Credits += money;
}


/***********************************************************************************************
 * HouseClass::Adjust_Capacity -- Adjusts the house Tiberium storage capacity.                 *
 *                                                                                             *
 *    Use this routine to adjust the maximum storage capacity for the house. This storage      *
 *    capacity will limit the number of Tiberium credits that can be stored at any one time.   *
 *                                                                                             *
 * INPUT:   adjust   -- The adjustment to the Tiberium storage capacity.                       *
 *                                                                                             *
 *          inanger  -- Is this a forced adjustment to capacity due to some hostile event?     *
 *                                                                                             *
 * OUTPUT:  Returns with the number of Tiberium credits lost.                                  *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/25/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
int HouseClass::Adjust_Capacity(int adjust, bool inanger)
{
	Validate();
	long oldcap = Capacity;
	int retval = 0;

	Capacity += adjust;
	Capacity = MAX(Capacity, 0L);
	if (Tiberium > Capacity) {
		retval = Tiberium - Capacity;
		Tiberium = Capacity;
		if (!inanger) {
			Refund_Money(retval);
			retval = 0;
		} else {
			IsMaxedOut = true;
		}
	}
	Silo_Redraw_Check(Tiberium, oldcap);
	return(retval);
}


/***********************************************************************************************
 * HouseClass::Silo_Redraw_Check -- Flags silos to be redrawn if necessary.                    *
 *                                                                                             *
 *    Call this routine when either the capacity or tiberium levels change for a house. This   *
 *    routine will determine if the aggregate tiberium storage level will result in the        *
 *    silos changing their imagery. If this is detected, then all the silos for this house     *
 *    are flagged to be redrawn.                                                               *
 *                                                                                             *
 * INPUT:   oldtib   -- Pre-change tiberium level.                                             *
 *                                                                                             *
 *          oldcap   -- Pre-change tiberium storage capacity.                                  *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/02/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Silo_Redraw_Check(long oldtib, long oldcap)
{
	Validate();
	int oldratio = 0;
	if (oldcap) oldratio = (oldtib * 5) / oldcap;
	int newratio = 0;
	if (Capacity) newratio = (Tiberium * 5) / Capacity;

	if (oldratio != newratio) {
		for (int index = 0; index < Buildings.Count(); index++) {
			BuildingClass * b = Buildings.Ptr(index);
			if (b && !b->IsInLimbo && b->House == this && *b == STRUCT_STORAGE) {
				b->Mark(MARK_CHANGE);
			}
		}
	}
}


/***********************************************************************************************
 * HouseClass::Read_INI -- Reads house specific data from INI.                                 *
 *                                                                                             *
 *    This routine reads the house specific data for a particular                              *
 *    scenario from the scenario INI file. Typical data includes starting                      *
 *    credits, maximum unit count, etc.                                                        *
 *                                                                                             *
 * INPUT:   buffer   -- Pointer to loaded scenario INI file.                                   *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/24/1994 JLB : Created.                                                                 *
 *   05/18/1995 JLB : Creates all houses.                                                      *
 *=============================================================================================*/
void HouseClass::Read_INI(char *buffer)
{
	HouseClass 	*p;				// Pointer to current player data.
	char const	*hname;			//	Pointer to house name.
	char			buf[128];

	for (HousesType index = HOUSE_FIRST; index < HOUSE_COUNT; index++) {
		hname = HouseTypeClass::As_Reference(index).IniName;
		int maxunit = WWGetPrivateProfileInt(hname, "MaxUnit", EACH_UNIT_MAX, buffer);

		maxunit = MAX(maxunit, 150);

		int maxbuilding = WWGetPrivateProfileInt(hname, "MaxBuilding", EACH_BUILDING_MAX, buffer);

		maxbuilding = MAX(maxbuilding, 150);

		int credits = WWGetPrivateProfileInt(hname, "Credits", 0, buffer);

		p = new HouseClass(index);

		p->MaxBuilding = maxbuilding;
		p->MaxUnit = maxunit;
		p->Credits = (long)credits * 100;
		p->InitialCredits = p->Credits;
		WWGetPrivateProfileString(hname, "Edge", "", buf, sizeof(buf)-1, buffer);
		p->Edge = Source_From_Name(buf);
		if (p->Edge == SOURCE_NONE) {
			p->Edge = SOURCE_NORTH;
		}

		if (GameToPlay == GAME_NORMAL) {
			WWGetPrivateProfileString(hname, "Allies", "", buf, sizeof(buf)-1, buffer);
			if (strlen(buf)) {
				char * tok = strtok(buf, ", \t");
				while (tok) {
					HousesType h = HouseTypeClass::From_Name(tok);
					p->Make_Ally(h);
					tok = strtok(NULL, ", \t");
				}

			} else {

				/*
				**	Special case for when no allies are specified in the INI file.
				**	The GDI side defaults to considering the neutral side to be
				**	friendly.
				*/
				if (p->Class->House == HOUSE_GOOD) {
					p->Make_Ally(HOUSE_NEUTRAL);
				}
			}
		}
	}
}


/***********************************************************************************************
 * HouseClass::Write_INI -- Writes house specific data into INI file.                          *
 *                                                                                             *
 *    Use this routine to write the house specific data (for all houses) into the INI file.    *
 *    It is used by the scenario editor when saving the scenario.                              *
 *                                                                                             *
 * INPUT:   buffer   -- INI file staging buffer.                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/28/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Write_INI(char *buffer)
{
	for (HousesType i = HOUSE_FIRST; i < HOUSE_COUNT; i++) {
		HouseClass * p = As_Pointer(i);

		if (p) {
			WWWritePrivateProfileInt(p->Class->IniName, "Credits", (int)(p->Credits / 100), buffer);
			WWWritePrivateProfileString(p->Class->IniName, "Edge", Name_From_Source(p->Edge), buffer);
			WWWritePrivateProfileInt(p->Class->IniName, "MaxUnit", p->MaxUnit, buffer);
			WWWritePrivateProfileInt(p->Class->IniName, "MaxBuilding", p->MaxBuilding, buffer);

			bool first = true;
			char sbuffer[100] = "";
			for (HousesType house = HOUSE_FIRST; house < HOUSE_COUNT; house++) {
				if (p->Is_Ally(house)) {
					if (!first) strcat(sbuffer, ",");
					strcat(sbuffer, As_Pointer(house)->Class->IniName);
					first = false;
				}
			}
			WWWritePrivateProfileString(p->Class->IniName, "Allies", sbuffer, buffer);
		}
	}
}


/***********************************************************************************************
 * HouseClass::Is_Ally -- Determines if the specified house is an ally.                        *
 *                                                                                             *
 *    This routine will determine if the house number specified is a ally to this house.       *
 *                                                                                             *
 * INPUT:   house -- The house number to check to see if it is an ally.                        *
 *                                                                                             *
 * OUTPUT:  Is the house an ally?                                                              *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Is_Ally(HousesType house) const
{
	Validate();
	if (house != HOUSE_NONE) {
		return(((1<<house) & Allies) != 0);
	}
	return(false);
}


/***********************************************************************************************
 * HouseClass::Is_Ally -- Determines if the specified house is an ally.                        *
 *                                                                                             *
 *    This routine will examine the specified house and determine if it is an ally.            *
 *                                                                                             *
 * INPUT:   house -- Pointer to the house object to check for ally relationship.               *
 *                                                                                             *
 * OUTPUT:  Is the specified house an ally?                                                    *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Is_Ally(HouseClass const * house) const
{
	Validate();
	if (house) {
		return(Is_Ally(house->Class->House));
	}
	return(false);
}


/***********************************************************************************************
 * HouseClass::Is_Ally -- Checks to see if the object is an ally.                              *
 *                                                                                             *
 *    This routine will examine the specified object and return whether it is an ally or not.  *
 *                                                                                             *
 * INPUT:   object   -- The object to examine to see if it is an ally.                         *
 *                                                                                             *
 * OUTPUT:  Is the specified object an ally?                                                   *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Is_Ally(ObjectClass const * object) const
{
	Validate();
	if (object) {
		return(Is_Ally(object->Owner()));
	}
	return(false);
}


/***********************************************************************************************
 * HouseClass::Make_Ally -- Make the specified house an ally.                                  *
 *                                                                                             *
 *    This routine will make the specified house an ally to this house. An allied house will   *
 *    not be considered a threat or potential target.                                          *
 *                                                                                             *
 * INPUT:   house -- The house to make an ally of this house.                                  *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *   08/08/1995 JLB : Breaks off combat when ally commences.                                   *
 *=============================================================================================*/
void HouseClass::Make_Ally(HousesType house)
{
	Validate();
	if (house != HOUSE_NONE && !Is_Ally(house)) {

		/*
		**	If in normal game play but the house is defeated, then don't allow the ally
		**	key to work.
		*/
		if (!ScenarioInit && (IsDefeated || house == HOUSE_JP)) return;

		Allies |= (1 << house);

#ifdef CHEAT_KEYS
		if (Debug_Flag) {
			HouseClass * enemy = HouseClass::As_Pointer(house);
			if (enemy && !enemy->Is_Ally(this)) {
				enemy->Make_Ally(Class->House);
			}
		}
#endif

		if ((Debug_Flag || GameToPlay != GAME_NORMAL) && !ScenarioInit) {
			char buffer[80];

			/*
			**	Sweep through all techno objects and perform a cheeseball tarcom clear to ensure
			**	that fighting will most likely stop when the cease fire begins.
			*/
			for (int index = 0; index < Logic.Count(); index++) {
				ObjectClass * object = Logic[index];

				if (object && !object->IsInLimbo && object->Owner() == Class->House) {
					TARGET target = ((TechnoClass *)object)->TarCom;
					if (Target_Legal(target) && As_Techno(target)) {
						if (Is_Ally(As_Techno(target))) {
							((TechnoClass *)object)->TarCom = TARGET_NONE;
						}
					}
				}
			}

			sprintf(buffer, Text_String(TXT_HAS_ALLIED), Name, HouseClass::As_Pointer(house)->Name);
			Messages.Add_Message(buffer, MPlayerTColors[RemapColor], TPF_6PT_GRAD|TPF_USE_GRAD_PAL|TPF_FULLSHADOW, 1200, 0, 0);
			Map.Flag_To_Redraw(false);
		}
	}
}


/***********************************************************************************************
 * HouseClass::Make_Enemy -- Make an enemy of the house specified.                             *
 *                                                                                             *
 *    This routine will flag the house specified so that it will be an enemy to this house.    *
 *    Enemy houses are legal targets for attack.                                               *
 *                                                                                             *
 * INPUT:   house -- The house to make an enemy of this house.                                 *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *   07/27/1995 JLB : Making war is a bilateral aaction.                                       *
 *=============================================================================================*/
void HouseClass::Make_Enemy(HousesType house)
{
	Validate();
	if (house != HOUSE_NONE && Is_Ally(house)) {
		HouseClass * enemy = HouseClass::As_Pointer(house);
		Allies &= ~(1 << house);
		if (enemy && enemy->Is_Ally(this)) {
			enemy->Allies &= ~(1 << Class->House);
		}

		if ((Debug_Flag || GameToPlay != GAME_NORMAL) && !ScenarioInit) {
			char buffer[80];

			sprintf(buffer, Text_String(TXT_AT_WAR), Name, enemy->Name);
			Messages.Add_Message(buffer, MPlayerTColors[RemapColor], TPF_6PT_GRAD|TPF_USE_GRAD_PAL|TPF_FULLSHADOW, 600, 0, 0);
			Map.Flag_To_Redraw(false);
		}
	}
}


/***********************************************************************************************
 * HouseClass::Remap_Table -- Fetches the remap table for this house object.                   *
 *                                                                                             *
 *    This routine will return with the remap table to use when displaying an object owned     *
 *    by this house. If the object is blushing (flashing), then the lightening remap table is  *
 *    always used. The "unit" parameter allows proper remap selection for those houses that    *
 *    have a different remap table for buildings or units.                                     *
 *                                                                                             *
 * INPUT:   blushing -- Is the object blushing (flashing)?                                     *
 *                                                                                             *
 *          unit     -- Is the object a vehicle or infantry?                                   *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the remap table to use when drawing this object.         *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
unsigned char const * HouseClass::Remap_Table(bool blushing, bool unit) const
{
	Validate();
	if (blushing) return(&Map.FadingLight[0]);

	/*
	** For normal game play, return the TypeClass's remap table for this
	** house type
	*/
	if (GameToPlay == GAME_NORMAL) {
		/*
		**	Special case exception for Nod and single player only. Remap
		**	buildings to red as opposed to the default color of bluegrey.
		*/
		if (!unit && Class->House == HOUSE_BAD) {
			return(RemapRed);
		}

		return(Class->RemapTable);
	} else {

		/*
		** For multiplayer games, return the remap table for this exact house instance.
		*/
		return(RemapTable);
	}
}


/***********************************************************************************************
 * HouseClass::Suggested_New_Team -- Determine what team should be created.                    *
 *                                                                                             *
 *    This routine examines the house condition and returns with the team that it thinks       *
 *    should be created. The units that are not currently a member of a team are examined      *
 *    to determine the team needed.                                                            *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the team type that should be created. If no team should  *
 *          be created, then NULL is returned.                                                 *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
TeamTypeClass const * HouseClass::Suggested_New_Team(bool alertcheck)
{
	Validate();
	if (GameToPlay == GAME_SKIRMISH && !IsHuman && alertcheck) {
		int combat = 0;
		int needed = 5;

		if (MPlayerAISkill == 0) {
			needed = 3;
		} else {
			if (MPlayerAISkill == 2) {
				needed = 5;
			}
		}

		if ((Map.MapCellWidth * Map.MapCellHeight) >= 4096) {
			needed = MIN(7, needed + 1);
		}

		for (int index = 0; index < Units.Count(); index++) {
			UnitClass * unit = Units.Ptr(index);

			if (unit && !unit->IsInLimbo && unit->House == this && unit->Strength > 0 && !unit->Team &&
				*unit != UNIT_HARVESTER && *unit != UNIT_MCV) {
				combat++;
			}
		}

		for (int index = 0; index < Infantry.Count(); index++) {
			InfantryClass * infantry = Infantry.Ptr(index);

			if (infantry && !infantry->IsInLimbo && infantry->House == this && infantry->Strength > 0 && !infantry->Team &&
				*infantry != INFANTRY_E7) {
				combat++;
			}
		}

		if (combat < needed) {
			return(NULL);
		}
	}
	return(TeamTypeClass::Suggested_New_Team(this, UScan, IScan, IsAlerted && alertcheck));
}


/***********************************************************************************************
 * HouseClass::Adjust_Threat -- Adjust threat for the region specified.                        *
 *                                                                                             *
 *    This routine is called when the threat rating for a region needs to change. The region   *
 *    and threat adjustment are provided.                                                      *
 *                                                                                             *
 * INPUT:   region   -- The region that adjustment is to occur on.                             *
 *                                                                                             *
 *          threat   -- The threat adjustment to perform.                                      *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Adjust_Threat(int region, int threat)
{
	Validate();
	static int _val[] = {
		-MAP_REGION_WIDTH - 1,	-MAP_REGION_WIDTH, -MAP_REGION_WIDTH + 1,
		-1,							0,						 1,
		MAP_REGION_WIDTH -1,		MAP_REGION_WIDTH,	 MAP_REGION_WIDTH +1
	};
	static int _thr[] = {
		2, 1, 2,
		1, 0, 1,
		2, 1,	2
	};
	int neg;
	int *val = &_val[0];
	int *thr = &_thr[0];

	if (threat < 0) {
		threat = -threat;
		neg = true;
	} else {
		neg = false;
	}

	for (int lp = 0; lp < 9; lp ++) {
		Regions[region + *val].Adjust_Threat(threat >> *thr, neg);
		val++;
		thr++;
	}
}


/***********************************************************************************************
 * HouseClass::Begin_Production -- Starts production of the specified object type.             *
 *                                                                                             *
 *    This routine is called from the event system. It will start production for the object    *
 *    type specified. This will be reflected in the sidebar as well as the house factory       *
 *    tracking variables.                                                                      *
 *                                                                                             *
 * INPUT:   type  -- The type of object to begin production on.                                *
 *                                                                                             *
 *          id    -- The subtype of object.                                                    *
 *                                                                                             *
 * OUTPUT:  Returns with the reason why, or why not, production was started.                   *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
ProdFailType HouseClass::Begin_Production(RTTIType type, int id)
{
	Validate();
	int * factory = 0;
	int result = true;
	bool initial_start = false;
	FactoryClass * fptr;
	TechnoTypeClass const * tech = Fetch_Techno_Type(type, id);

	fprintf(stderr, "[ERR] Begin_Production: type=%d id=%d tech=%p house=%s\n",
		(int)type, id, (const void*)tech, Class ? Class->IniName : "?");

	switch (type) {
		case RTTI_AIRCRAFT:
		case RTTI_AIRCRAFTTYPE:
			factory = &AircraftFactory;
			break;

		case RTTI_UNIT:
		case RTTI_UNITTYPE:
			factory = &UnitFactory;
			break;

		case RTTI_BUILDING:
		case RTTI_BUILDINGTYPE:
			factory = &BuildingFactory;
			break;

		case RTTI_INFANTRY:
		case RTTI_INFANTRYTYPE:
			factory = &InfantryFactory;
			break;

		case RTTI_SPECIAL:
			factory = &SpecialFactory;
			break;
	}

	/*
	**	Check for legality of the production object type suggested.
	*/
	if (!factory) {
		fprintf(stderr, "[ERR] Begin_Production: PROD_ILLEGAL — no factory ptr for type\n");
		return(PROD_ILLEGAL);
	}

	/*
	**	If the house is already busy producing the requested object, then
	**	return with this failure code, unless we are restarting production.
	*/
	if (*factory != -1) {
		fptr = Factories.Raw_Ptr(*factory);
		if (fptr->Is_Building()) {
			fprintf(stderr, "[ERR] Begin_Production: PROD_CANT — factory already building\n");
			return(PROD_CANT);
		}
	} else {
		fptr = new FactoryClass();
		if (!fptr) {
			fprintf(stderr, "[ERR] Begin_Production: PROD_CANT — alloc failed\n");
			return(PROD_CANT);
		}
		*factory = Factories.ID(fptr);
		result = (tech) ? fptr->Set(*tech, *this) : fptr->Set(id, *this);
		fprintf(stderr, "[ERR] Begin_Production: Set() returned %d, factory_id=%d\n", result, *factory);
		initial_start = true;
	}

	if (result) {
		bool started = fptr->Start();
		fprintf(stderr, "[ERR] Begin_Production: Start()=%d, linking factory %d\n",
			started ? 1 : 0, *factory);

		/*
		**	Link this factory to the sidebar so that proper graphic feedback
		**	can take place.
		*/
		if (PlayerPtr == this) {
			Map.Factory_Link(*factory, type, id);
		}

		return(PROD_OK);
	}
	fprintf(stderr, "[ERR] Begin_Production: PROD_CANT — Set() failed\n");
	return(PROD_CANT);
}


/***********************************************************************************************
 * HouseClass::Suspend_Production -- Temporarily puts production on hold.                      *
 *                                                                                             *
 *    This routine is called from the event system whenever the production of the specified    *
 *    type needs to be suspended. The suspended production will be reflected in the sidebar    *
 *    as well as in the house control structure.                                               *
 *                                                                                             *
 * INPUT:   type  -- The type of object that production is being suspended for.                *
 *                                                                                             *
 * OUTPUT:  Returns why, or why not, production was suspended.                                 *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
ProdFailType HouseClass::Suspend_Production(RTTIType type)
{
	Validate();
	int * factory = 0;

	switch (type) {
		case RTTI_AIRCRAFT:
		case RTTI_AIRCRAFTTYPE:
			factory = &AircraftFactory;
			break;

		case RTTI_UNIT:
		case RTTI_UNITTYPE:
			factory = &UnitFactory;
			break;

		case RTTI_BUILDING:
		case RTTI_BUILDINGTYPE:
			factory = &BuildingFactory;
			break;

		case RTTI_INFANTRY:
		case RTTI_INFANTRYTYPE:
			factory = &InfantryFactory;
			break;

		case RTTI_SPECIAL:
			factory = &SpecialFactory;
			break;
	}

	/*
	**	Check for legality of the production object type suggested.
	*/
	if (!factory) return(PROD_ILLEGAL);

	/*
	**	If the house is already busy producing the requested object, then
	**	return with this failure code.
	*/
	if (*factory == -1) return(PROD_CANT);

	/*
	**	Create the factory pointer object.
	**	If the factory could not be created, then report this error condition.
	*/
	FactoryClass * fptr = Factories.Raw_Ptr(*factory);
	if (!fptr) return(PROD_CANT);

	/*
	**	Actually suspend the production.
	*/
	fptr->Suspend();

	/*
	**	Tell the sidebar that it needs to be redrawn because of this.
	*/
	if (PlayerPtr == this) {
		Map.SidebarClass::IsToRedraw = true;
		Map.SidebarClass::Column[0].IsToRedraw = true;
		Map.SidebarClass::Column[1].IsToRedraw = true;
		Map.Flag_To_Redraw(false);
	}

	return(PROD_OK);
}


/***********************************************************************************************
 * HouseClass::Abandon_Production -- Abandons production of item type specified.               *
 *                                                                                             *
 *    This routine is called from the event system whenever production must be abandoned for   *
 *    the type specified. This will remove the factory and pending object from the sidebar as  *
 *    well as from the house factory record.                                                   *
 *                                                                                             *
 * INPUT:   type  -- The object type that production is being suspended for.                   *
 *                                                                                             *
 * OUTPUT:  Returns the reason why or why not, production was suspended.                       *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/08/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
ProdFailType HouseClass::Abandon_Production(RTTIType type)
{
	Validate();
	int * factory = 0;

	switch (type) {
		case RTTI_AIRCRAFT:
		case RTTI_AIRCRAFTTYPE:
			factory = &AircraftFactory;
			break;

		case RTTI_UNIT:
		case RTTI_UNITTYPE:
			factory = &UnitFactory;
			break;

		case RTTI_BUILDING:
		case RTTI_BUILDINGTYPE:
			factory = &BuildingFactory;
			break;

		case RTTI_INFANTRY:
		case RTTI_INFANTRYTYPE:
			factory = &InfantryFactory;
			break;

		case RTTI_SPECIAL:
			factory = &SpecialFactory;
			break;
	}

	/*
	**	Check for legality of the production object type suggested.
	*/
	if (!factory) return(PROD_ILLEGAL);

	/*
	**	If there is no factory to abandon, then return with a failure code.
	*/
	if (*factory == -1) return(PROD_CANT);

	/*
	**	Fetch the factory pointer object.
	*/
	FactoryClass * fptr = Factories.Raw_Ptr(*factory);
	if (!fptr) return(PROD_CANT);

	/*
	**	Tell the sidebar that it needs to be redrawn because of this.
	*/
	if (PlayerPtr == this) {
		Map.Abandon_Production(type, *factory);

		if (type == RTTI_BUILDINGTYPE || type == RTTI_BUILDING) {
			Map.PendingObjectPtr = 0;
			Map.PendingObject = 0;
			Map.PendingHouse = HOUSE_NONE;
			Map.Set_Cursor_Shape(0);
		}
	}

	/*
	**	Abandon production of the object.
	*/
	fptr->Abandon();
	delete fptr;
	*factory = -1;

	return(PROD_OK);
}


/***********************************************************************************************
 * HouseClass::Special_Weapon_AI -- Fires special weapon.                                      *
 *                                                                                             *
 *    This routine will pick a good target to fire the special weapon specified.               *
 *                                                                                             *
 * INPUT:   id -- The special weapon id to fire.                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/24/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Special_Weapon_AI(SpecialWeaponType id)
{
	Validate();
	/*
	** Loop through all of the building objects on the map
	** and see which ones are available.
 	*/
	BuildingClass * bestptr = NULL;
	int best = -1;

	for (int index = 0; index < Buildings.Count(); index++) {
		BuildingClass * b = Buildings.Ptr(index);

		/*
		** If the building is valid, not in limbo, not in the process of
		** being destroyed and not our ally, then we can consider it.
		*/
		if (b && !b->IsInLimbo && b->Strength && !Is_Ally(b)) {
			if (b->Value() > best || best == -1) {
				best = b->Value();
				bestptr = b;
			}
		}
	}

	if (bestptr) {
		CELL cell = Coord_Cell(bestptr->Center_Coord());
		Place_Special_Blast(id, cell);
	}
}


/***********************************************************************************************
 * HouseClass::Place_Special_Blast -- Place a special blast effect at location specified.      *
 *                                                                                             *
 *    This routine will create a blast effect at the cell specified. This is the result of     *
 *    the special weapons.                                                                     *
 *                                                                                             *
 * INPUT:   id    -- The special weapon id number.                                             *
 *                                                                                             *
 *          cell  -- The location where the special weapon attack is to occur.                 *
 *                                                                                             *
 * OUTPUT:  Was the special weapon successfully fired at the location specified?               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : commented.                                                               *
 *   07/25/1995 JLB : Added scatter effect for nuclear bomb.                                   *
 *   07/28/1995 JLB : Revamped to use super weapon class controller.                           *
 *=============================================================================================*/
bool HouseClass::Place_Special_Blast(SpecialWeaponType id, CELL cell)
{
	Validate();
	BuildingClass * launchsite = 0;
	AnimClass * anim = 0;
	int index;
	switch (id) {

		case SPC_ION_CANNON:
			if (IonCannon.Is_Ready()) {
				anim = new AnimClass(ANIM_ION_CANNON, Cell_Coord(cell));
				if (anim) anim->Owner = Class->House;
				if (this == PlayerPtr) {
					Map.IsTargettingMode = false;
				}
				IonCannon.Discharged(PlayerPtr == this);
				IsRecalcNeeded = true;
			}
			break;

		case SPC_NUCLEAR_BOMB:
			if (NukeStrike.Is_Ready()) {


#ifdef NEVER
				/*
				**	Scatter the nuclear bomb impact point into an adjacent cell.
				*/
				for (;;) {
					CELL newcell = Adjacent_Cell(cell, Random_Pick(FACING_N, FACING_COUNT));
					if (Map.In_Radar(newcell)) {
						cell = newcell;
						break;
					}
				}
#endif

				/*
				**	Search for a suitable launch site for this missile.
				*/
				for (int index = 0; index < Buildings.Count(); index++) {
					BuildingClass * b = Buildings.Ptr(index);
					if (b && !b->IsInLimbo && b->House == this && *b == STRUCT_TEMPLE) {
						launchsite = b;
						break;
					}
				}

				/*
				**	If a launch site was found, then proceed with the normal launch
				**	sequence.
				*/
				if (launchsite) {
					launchsite->Assign_Mission(MISSION_MISSILE);
					launchsite->Commence();
					NukeDest = cell;
					NukePieces = 0;

				} else {

					/*
					**	Only in the multiplayer version can the nuclear bomb be
					**	sent from some off screen source.
					*/
					if (GameToPlay == GAME_NORMAL) return(false);

					/*
					**	Since no launch site was found, just bring the missile in
					**	directly from the North map edge.
					*/
				 	BulletClass *bullet = new BulletClass(BULLET_NUKE_DOWN);
					if (bullet) {
						COORDINATE start = Cell_Coord(XY_Cell(Cell_X(cell), 0));
						bullet->Assign_Target(::As_Target(cell));
						bullet->Payback = NULL;
						bullet->Strength = 1;
						if (!bullet->Unlimbo(start, DIR_S)) {
							delete bullet;
						} else {
							bullet->PrimaryFacing.Set_Current(DIR_S);
						}
						Speak(VOX_INCOMING_NUKE);
						Sound_Effect(VOC_NUKE_FIRE, start);
					}
				}
				if (this == PlayerPtr) {
					Map.IsTargettingMode = false;
				}
				NukeStrike.Discharged(this == PlayerPtr);
				IsRecalcNeeded = true;
			}
			break;

		case SPC_AIR_STRIKE:
			if (AirStrike.Is_Ready()) {
				int strike = 1;
				if (GameToPlay == GAME_NORMAL) {
					strike = Bound(BuildLevel/3, 1, 3);
				} else {
					strike = Bound(MPlayerUnitCount/5, 1, 3);
				}
				Create_Air_Reinforcement(this, AIRCRAFT_A10, strike, MISSION_HUNT, ::As_Target(cell), TARGET_NONE);
				if (this == PlayerPtr) {
					Map.IsTargettingMode = false;
				}
				AirStrike.Discharged(this == PlayerPtr);
				IsRecalcNeeded = true;
			}
			break;
	}
	return(true);
}


/***********************************************************************************************
 * HouseClass::Place_Object -- Places the object (building) at location specified.             *
 *                                                                                             *
 *    This routine is called when a building has been produced and now must be placed on       *
 *    the map. When the player clicks on the map, this routine is ultimately called when the   *
 *    event passes through the event queue system.                                             *
 *                                                                                             *
 * INPUT:   type  -- The object type to place. The actual object is lifted from the sidebar.   *
 *                                                                                             *
 *                                                                                             *
 *          cell  -- The location to place the object on the map.                              *
 *                                                                                             *
 * OUTPUT:  Was the placement successful?                                                      *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Place_Object(RTTIType type, CELL cell)
{
	Validate();
	TechnoClass * tech = 0;
	FactoryClass * factory = 0;

	switch (type) {
		case RTTI_AIRCRAFT:
		case RTTI_AIRCRAFTTYPE:
			if (AircraftFactory != -1) {
				factory = Factories.Raw_Ptr(AircraftFactory);
			}
			break;

		case RTTI_INFANTRY:
		case RTTI_INFANTRYTYPE:
			if (InfantryFactory != -1) {
				factory = Factories.Raw_Ptr(InfantryFactory);
			}
			break;

		case RTTI_UNIT:
		case RTTI_UNITTYPE:
			if (UnitFactory != -1) {
				factory = Factories.Raw_Ptr(UnitFactory);
			}
			break;

		case RTTI_BUILDING:
		case RTTI_BUILDINGTYPE:
			if (BuildingFactory != -1) {
				factory = Factories.Raw_Ptr(BuildingFactory);
			}
			break;
	}

	/*
	**	Only if there is a factory active for this type, can it be "placed".
	**	In the case of a missing factory, then this request is completely bogus --
	**	ignore it. This might occur if, between two events to exit the same
	**	object, the mouse was clicked on the sidebar to start building again.
	**	The second placement event should NOT try to place the object that is
	**	just starting construction.
	*/
	if (factory && factory->Has_Completed()) {
		tech = factory->Get_Object();

		if (cell == -1) {
			TechnoClass * pending = factory->Get_Object();
			if (pending) {
				TechnoClass * builder = pending->Who_Can_Build_Me(false, false);

				if (builder && builder->Exit_Object(pending)) {

					/*
					**	Since the object has left the factory under its own power, delete
					**	the production manager tied to this slot in the sidebar. Its job
					**	has been completed.
					*/
					factory->Completed();
					Abandon_Production(type);
				} else {

					/*
					**	The object could not leave under it's own power. Just wait
					**	until the player tries to place the object again.
					*/
					return(false);
				}
			}

		} else {

			if (tech) {
				TechnoClass * builder = tech->Who_Can_Build_Me(false, false);
				if (builder) {

					/*
					**	Ensures that the proximity check is performed even when the building is
					**	placed by way of a remote event.
					*/
					if (tech->What_Am_I() != RTTI_BUILDING || ((BuildingClass *)tech)->Passes_Proximity_Check(cell)) {
						builder->Transmit_Message(RADIO_HELLO, tech);
						if (tech->Unlimbo(Cell_Coord(cell))) {
							factory->Completed();
							Abandon_Production(type);

							if (PlayerPtr == this) {
								Sound_Effect(VOC_SLAM);
								Map.Set_Cursor_Shape(0);
								Map.PendingObjectPtr = 0;
								Map.PendingObject = 0;
								Map.PendingHouse = HOUSE_NONE;
							}
							return(true);
						} else {
							if (this == PlayerPtr) {
								Speak(VOX_DEPLOY);
							}
						}
						builder->Transmit_Message(RADIO_OVER_OUT);
					}
				}
				return(false);

			} else {

				// Play a bad sound here?
				return(false);
			}
		}
	}

	return(true);
}


/***********************************************************************************************
 * HouseClass::Manual_Place -- Inform display system of building placement mode.               *
 *                                                                                             *
 *    This routine will inform the display system that building placement mode has begun.      *
 *    The cursor will be created that matches the layout of the building shape.                *
 *                                                                                             *
 * INPUT:   builder  -- The factory that is building this object.                              *
 *                                                                                             *
 *          object   -- The building that is going to be placed down on the map.               *
 *                                                                                             *
 * OUTPUT:  Was the building placement mode successfully initiated?                            *
 *                                                                                             *
 * WARNINGS:   This merely adjusts the cursor shape. Nothing that affects networked games      *
 *             is affected.                                                                    *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/04/1995 JLB : Created.                                                                 *
 *   05/30/1995 JLB : Uses the Bib_And_Offset() function to determine bib size.                *
 *=============================================================================================*/
bool HouseClass::Manual_Place(BuildingClass * builder,  BuildingClass * object)
{
	Validate();
	if (this == PlayerPtr && !Map.PendingObject && builder && object) {

		/*
		**	Ensures that object selection doesn't remain when
		**	building placement takes place.
		*/
		Unselect_All();

		Map.Repair_Mode_Control(0);
		Map.Sell_Mode_Control(0);

		Map.PendingObject = object->Class;
		Map.PendingObjectPtr = object;
		Map.PendingHouse = Class->House;

		Map.Set_Cursor_Shape(object->Occupy_List(true));
		Map.Set_Cursor_Pos(Coord_Cell(builder->Coord));
		builder->Mark(MARK_CHANGE);
		return(true);
	}
	return(false);
}


#ifdef OBSOLETE
/***********************************************************************************************
 * HouseClass::Init_Ion_Cannon -- Initialize the ion cannon countdown.                         *
 *                                                                                             *
 *    This routine will initiate the ion cannon charging countdown. It will add the ion        *
 *    cannon to the sidebar if it isn't there and it is specified to be added.                 *
 *                                                                                             *
 * INPUT:   first_time  -- Set to true if the ion cannon must be added to the sidebar.         *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : commented                                                                *
 *=============================================================================================*/
void HouseClass::Init_Ion_Cannon(SpecialControlType control)
{
	Validate();
	switch (control) {
		case CONTROL_RESET:
			if (IonCannonPresent) {
				IonOldStage = -1;
				IonControl.Set(ION_CANNON_GONE_TIME);
				if (PlayerPtr == this) {
					Map.Add(RTTI_SPECIAL, SPC_ION_CANNON);
					if (!ScenarioInit) {
						Speak(VOX_ION_CHARGING);
					}
				}
			}
			break;

		/*
		**	Adds the special no-prerequisite ion cannon option.
		*/
		case CONTROL_ONE_TIME:
			if (!IonCannonPresent) {
				Init_Ion_Cannon(CONTROL_ADD);
				IonOneTimeFlag = true;
			}
			break;

		/*
		**	Adds the normal legitimate ion cannon option. If there was
		**	already a one-time ion cannon available, the charging state
		**	is NOT interrupted.
		*/
		case CONTROL_ADD:
			IonOneTimeFlag = false;
			if (!IconCannonPresent) {
				IonCannonPresent = true;
				IonReady = false;
				Init_Ion_Cannon(CONTROL_RESET);
			}
			break;

		case CONTROL_REMOVE:
			break;
	}


	if (!(first_time && IonCannonPresent)) {

		if (IonCannonPresent && IonOneTimeFlag) {
			IonOneTimeFlag = false;
			if (this == PlayerPtr) Map.Recalc();
			return;
		}

		if (first_time && this == PlayerPtr) {
			Map.Add(RTTI_SPECIAL, SPC_ION_CANNON);
		}

		if (!ScenarioInit) {
			if (this == PlayerPtr) {
				Speak(VOX_ION_CHARGING);
			}
		}

		IonControl.Set(ION_CANNON_GONE_TIME);
		IonCannonPresent = true;
		IonReady = false;
		IonOldStage = -1;
		IonOneTimeFlag = one_time_effect;
	} else {
	 	if (first_time && IonCannonPresent && !one_time_effect && IonOneTimeFlag) {
			IonOneTimeFlag	= false;
		}
	}
}
#ifdef NEVER
void HouseClass::Init_Ion_Cannon(bool first_time, bool one_time_effect)
{
	Validate();
	if (!(first_time && IonCannonPresent)) {

		if (IonCannonPresent && IonOneTimeFlag) {
			IonOneTimeFlag = false;
			if (this == PlayerPtr) Map.Recalc();
			return;
		}

		if (first_time && this == PlayerPtr) {
			Map.Add(RTTI_SPECIAL, SPC_ION_CANNON);
		}

		if (!ScenarioInit) {
			if (this == PlayerPtr) {
				Speak(VOX_ION_CHARGING);
			}
		}

		IonControl.Set(ION_CANNON_GONE_TIME);
		IonCannonPresent = true;
		IonReady = false;
		IonOldStage = -1;
		IonOneTimeFlag = one_time_effect;
	} else {
	 	if (first_time && IonCannonPresent && !one_time_effect && IonOneTimeFlag) {
			IonOneTimeFlag	= false;
		}
	}
}
#endif


/***********************************************************************************************
 * HouseClass::Remove_Ion_Cannon -- Disables the ion cannon.                                   *
 *                                                                                             *
 *    This routine will disable the ion cannon. It is called when the ion cannon cannot        *
 *    establish a command link to the ground (usually when there is insufficient power).       *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : commented                                                                *
 *=============================================================================================*/
void HouseClass::Remove_Ion_Cannon(void)
{
	Validate();
	if (IonCannonPresent) {
		IonCannonPresent = false;
		IonOneTimeFlag = false;
		IonReady = false;
		IonControl.Clear();
		IonOldStage = -1;
	}
}
#endif


/***************************************************************************
 * HouseClass::Clobber_All -- removes house & all its objects              *
 *                                                                         *
 * INPUT:                                                                  *
 *      none.                                                              *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      none.                                                              *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/16/1995 BRR : Created.                                             *
 *   06/09/1995 JLB : Handles aircraft.                                    *
 *=========================================================================*/
void HouseClass::Clobber_All(void)
{
	Validate();
	int i;

	for (i = 0; i < ::Aircraft.Count(); i++) {
		if (::Aircraft.Ptr(i)->House == this) {
			delete ::Aircraft.Ptr(i);
			i--;
		}
	}
	for (i = 0; i < ::Units.Count(); i++) {
		if (::Units.Ptr(i)->House == this) {
			delete ::Units.Ptr(i);
			i--;
		}
	}
	for (i = 0; i < Infantry.Count(); i++) {
		if (Infantry.Ptr(i)->House == this) {
			delete Infantry.Ptr(i);
			i--;
		}
	}
	for (i = 0; i < Buildings.Count(); i++) {
		if (Buildings.Ptr(i)->House == this) {
			delete Buildings.Ptr(i);
			i--;
		}
	}
	for (i = 0; i < TeamTypes.Count(); i++) {
		if (TeamTypes.Ptr(i)->House == Class->House) {
			delete TeamTypes.Ptr(i);
			i--;
		}
	}
	for (i = 0; i < Triggers.Count(); i++) {
		if (Triggers.Ptr(i)->House == Class->House) {
			delete Triggers.Ptr(i);
			i--;
		}
	}

	delete this;
}


#ifdef NEVER
/***********************************************************************************************
 * HouseClass::Init_Nuke_Bomb -- Adds (if necessary) the atom bomb to the sidebar.             *
 *                                                                                             *
 *    Use this routine whenever a piece of atom bomb has been discovered (also at scenario     *
 *    start). It will add the nuclear bomb button to the sidebar if necessary.                 *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : commented                                                                *
 *=============================================================================================*/
void HouseClass::Init_Nuke_Bomb(bool first_time, bool one_time_effect)
{
	Validate();
	if (!first_time || !NukePresent) {

		if (NukePresent && NukeOneTimeFlag) {
			NukeOneTimeFlag = false;
			if (this == PlayerPtr) Map.Recalc();
			return;
		}

		if (first_time && this == PlayerPtr) {
			Map.Add(RTTI_SPECIAL, SPC_NUCLEAR_BOMB);
		}

		NukeControl.Set(NUKE_GONE_TIME);
		NukePresent = true;
		NukeReady = false;
		NukeOldStage = -1;
		NukeOneTimeFlag = one_time_effect;

	} else {
		if (!one_time_effect && NukeOneTimeFlag) {
			NukeOneTimeFlag = false;
		}
	}
}


/***********************************************************************************************
 * HouseClass::Remove_Nuke_Bomb -- Removes the nuclear bomb from the sidebar.                  *
 *                                                                                             *
 *    This routine will remove the nuclear bomb from the sidebar. It should be called when     *
 *    the nuclear strike has been launched.                                                    *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : commented                                                                *
 *   07/25/1995 JLB : Handles recharge reset logic.                                            *
 *=============================================================================================*/
void HouseClass::Remove_Nuke_Bomb(void)
{
	Validate();
	if (NukePresent && !NukeOneTimeFlag) {
		NukePresent = false;
		NukeControl.Clear();
		NukeOldStage = -1;
		NukeReady = false;
	}
}


/***********************************************************************************************
 * HouseClass::Init_Air_Strike -- Add (or reset) the air strike sidebar button.                *
 *                                                                                             *
 *    This routine will activate (add if so indicated) the air strike button to the sidebar.   *
 *    Call this routine when events indicate that a special air strike is available.           *
 *                                                                                             *
 * INPUT:   first_time  -- If the air strike button is to be added, then this will be true.    *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : commented                                                                *
 *=============================================================================================*/
void HouseClass::Init_Air_Strike(bool first_time, bool one_time_effect)
{
	Validate();
	if (!(first_time && AirPresent)) {

		if (AirPresent && AirOneTimeFlag) {
			AirOneTimeFlag = false;
			AirPresent		= false;
			Map.Recalc();
			return;
		}

		if (first_time) {
			if (PlayerPtr == this) {
				Map.Add(RTTI_SPECIAL, SPC_AIR_STRIKE);
			}
			AirControl.Set(0);
		} else {
			AirControl.Set(AIR_CANNON_GONE_TIME);
		}

		AirReady 			= first_time;
		AirPresent 			= true;
		AirOldStage 		= -1;
		AirOneTimeFlag		= one_time_effect;

		if (AirReady && !IsHuman) {
			Special_Weapon_AI(SPC_AIR_STRIKE);
		}
	} else {
	 	if (first_time && AirPresent && !one_time_effect && AirOneTimeFlag) {
			AirOneTimeFlag	= false;
		}
	}
}


/***********************************************************************************************
 * HouseClass::Remove_Air_Strike -- Removes the air strike button from the sidebar.            *
 *                                                                                             *
 *    This routine will remove the air strike button from the sidebar. Call this routine when  *
 *    the air strike has been launched.                                                        *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : commented                                                                *
 *=============================================================================================*/
void HouseClass::Remove_Air_Strike(void)
{
	Validate();
	AirPresent = false;
	AirReady = false;
	AirControl.Clear();
	AirOldStage = -1;
}


/***********************************************************************************************
 * HouseClass::Make_Air_Strike_Available -- Make the airstrike available.                      *
 *                                                                                             *
 *    This routine will make the airstrike available. Typically, this results in a new icon    *
 *    added to the sidebar.                                                                    *
 *                                                                                             *
 * INPUT:   present  -- The the airstrike being added?                                         *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/20/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Make_Air_Strike_Available(bool present, bool one_time_effect)
{
	Validate();
	Init_Air_Strike(true, one_time_effect);
	AirPresent = present;
}
#endif


/***********************************************************************************************
 * HouseClass::Add_Nuke_Piece -- Add a nuclear piece to the collection.                        *
 *                                                                                             *
 *    This routine will a the specified nuclear piece to the house collection of parts. When   *
 *    all the pieces have been added, a nuclear strike ability is made available.              *
 *                                                                                             *
 * INPUT:   piece -- The nuclear piece to add. If equal to "-1", then the next possible piece  *
 *                   is added.                                                                 *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/20/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Add_Nuke_Piece(int piece)
{
	Validate();
	if (piece == -1) {
		piece = 1;
		if (!(NukePieces & 0x01)) {
			piece = 1;
		}
		if (!(NukePieces & 0x02)) {
			piece = 2;
		}
		if (!(NukePieces & 0x04)) {
			piece = 3;
		}
	}
	NukePieces |= 1 << (piece - 1);
//	Init_Nuke_Bomb(false);
}


/***********************************************************************************************
 * HouseClass::Detach -- Removes specified object from house tracking systems.                 *
 *                                                                                             *
 *    This routine is called when an object is to be removed from the game system. If the      *
 *    specified object is part of the house tracking system, then it will be removed.          *
 *                                                                                             *
 * INPUT:   target   -- The target value of the object that is to be removed from the game.    *
 *                                                                                             *
 *          all      -- Is the target going away for good as opposed to just cloaking/hiding?  *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/18/1995 JLB : commented                                                                *
 *=============================================================================================*/
void HouseClass::Detach(TARGET, bool )
{
	Validate();
//	if (LaunchSite == target) {
//		LaunchSite = TARGET_NONE;
//	}
}


/***********************************************************************************************
 * HouseClass::Does_Enemy_Building_Exist -- Checks for enemy building of specified type.       *
 *                                                                                             *
 *    This routine will examine the enemy houses and if there is a building owned by one       *
 *    of those house, true will be returned.                                                   *
 *                                                                                             *
 * INPUT:   btype -- The building type to check for.                                           *
 *                                                                                             *
 * OUTPUT:  Does a building of the specified type exist for one of the enemy houses?           *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Does_Enemy_Building_Exist(StructType btype) const
{
	Validate();
	int bflag = 1L << btype;
	for (HousesType index = HOUSE_FIRST; index < HOUSE_COUNT; index++) {
		HouseClass * house = HouseClass::As_Pointer(index);

		if (house && !Is_Ally(house) && (house->BScan & bflag) != 0) {
			return(true);
		}
	}
	return(false);
}


/***********************************************************************************************
 * HouseClass::Suggest_New_Object -- Determine what would the next buildable object be.        *
 *                                                                                             *
 *    This routine will examine the house status and return with a techno type pointer to      *
 *    the object type that it thinks should be created. The type is restricted to match the    *
 *    type specified. Typical use of this routine is by computer controlled factories.         *
 *                                                                                             *
 * INPUT:   objecttype  -- The type of object to restrict the scan for.                        *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to a techno type for the object type that should be         *
 *          created. If no object should be created, then NULL is returned.                    *
 *                                                                                             *
 * WARNINGS:   This is a time consuming routine. Only call when necessary.                     *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
TechnoTypeClass const * HouseClass::Suggest_New_Object(RTTIType objecttype) const
{
	Validate();
	TechnoTypeClass const * techno = NULL;
	long reserve = 0;
	StructType reserve_build = STRUCT_NONE;
	long flags = ActiveBScan;
	int combat_units = 0;
	int helipad_count = 0;
	int combat_aircraft_count = 0;
	int enemy_aircraft_count = 0;
	int anti_air_count = 0;
	bool recovery_mode = false;
	bool economy_harassed = false;
	int personality = 1;

	if (GameToPlay == GAME_SKIRMISH && !IsHuman) {
		int refinery_count = 0;
		int harvester_count = 0;
		int defense_count = 0;
		int valuable_count = 0;
		int desired_defense_count = 0;
		bool big_map = ((Map.MapCellWidth * Map.MapCellHeight) >= 4096);
		bool huge_map = ((Map.MapCellWidth * Map.MapCellHeight) >= 6144);
		personality = Skirmish_AI_Personality(this);
		bool under_pressure = (WhoLastHurtMe != HOUSE_NONE && WhoLastHurtMe != Class->House);

		if (flags & STRUCTF_ADVANCED_POWER) flags |= STRUCTF_POWER;
		if (flags & STRUCTF_HAND) flags |= STRUCTF_BARRACKS;
		if (flags & STRUCTF_AIRSTRIP) flags |= STRUCTF_WEAP;
		if (flags & STRUCTF_OBELISK) flags |= STRUCTF_ATOWER;

		for (int index = 0; index < Buildings.Count(); index++) {
			BuildingClass * building = Buildings.Ptr(index);

			if (building && !building->IsInLimbo && building->House == this && building->Strength > 0) {
				if (*building == STRUCT_REFINERY) {
					refinery_count++;
				}
				if (*building == STRUCT_REFINERY || *building == STRUCT_WEAP || *building == STRUCT_AIRSTRIP ||
					*building == STRUCT_REPAIR || *building == STRUCT_RADAR || *building == STRUCT_EYE ||
					*building == STRUCT_TEMPLE) {
					valuable_count++;
				}
				if (*building == STRUCT_HELIPAD) {
					helipad_count++;
				}
				if (*building == STRUCT_GTOWER || *building == STRUCT_ATOWER || *building == STRUCT_TURRET ||
					*building == STRUCT_OBELISK || *building == STRUCT_SAM) {
					defense_count++;
				}
				if (*building == STRUCT_ATOWER || *building == STRUCT_SAM) {
					anti_air_count++;
				}
			}
		}

		for (int index = 0; index < Units.Count(); index++) {
			UnitClass * unit = Units.Ptr(index);

			if (unit && !unit->IsInLimbo && unit->House == this && unit->Strength > 0) {
				if (*unit == UNIT_HARVESTER) {
					harvester_count++;
				} else if (*unit != UNIT_MCV) {
					combat_units++;
				}
			}
		}

		for (int index = 0; index < Aircraft.Count(); index++) {
			AircraftClass * aircraft = Aircraft.Ptr(index);

			if (aircraft && !aircraft->IsInLimbo && aircraft->Strength > 0) {
				if (aircraft->House == this) {
					if (*aircraft != AIRCRAFT_TRANSPORT && *aircraft != AIRCRAFT_CARGO && *aircraft != AIRCRAFT_A10) {
						combat_aircraft_count++;
					}
				} else if (!Is_Ally(aircraft)) {
					if (*aircraft != AIRCRAFT_TRANSPORT && *aircraft != AIRCRAFT_CARGO && *aircraft != AIRCRAFT_A10) {
						enemy_aircraft_count++;
					}
				}
			}
		}

		recovery_mode = Skirmish_AI_Is_Recovery_Mode(flags, Power, Drain, refinery_count, combat_units);
		economy_harassed = Skirmish_AI_Economy_Harassed(flags, under_pressure, refinery_count, harvester_count, combat_units);

		if (!(flags & STRUCTF_POWER)) {
			reserve_build = STRUCT_POWER;
		} else if (!(flags & STRUCTF_REFINERY)) {
			reserve_build = STRUCT_REFINERY;
		} else if (ActLike == HOUSE_GOOD && !(flags & STRUCTF_BARRACKS)) {
			reserve_build = STRUCT_BARRACKS;
		} else if (ActLike == HOUSE_BAD && !(flags & STRUCTF_HAND)) {
			reserve_build = STRUCT_HAND;
		} else if (ActLike == HOUSE_GOOD && !(flags & STRUCTF_WEAP)) {
			reserve_build = STRUCT_WEAP;
		} else if (ActLike == HOUSE_BAD && !(flags & STRUCTF_AIRSTRIP)) {
			reserve_build = STRUCT_AIRSTRIP;
		} else if (!(flags & STRUCTF_STORAGE) && Capacity > 0 &&
			Tiberium > (huge_map ? ((Capacity * 2) / 3) : ((Capacity * 3) / 4))) {
			reserve_build = STRUCT_STORAGE;
		} else if (economy_harassed && refinery_count < 2 && harvester_count >= MAX(2, refinery_count)) {
			reserve_build = STRUCT_REFINERY;
		} else if ((huge_map && refinery_count < 3 && harvester_count >= MAX(2, refinery_count)) ||
			(big_map && refinery_count < 2 && harvester_count >= MAX(2, refinery_count))) {
			reserve_build = STRUCT_REFINERY;
		} else if (!(flags & STRUCTF_RADAR)) {
			reserve_build = STRUCT_RADAR;
		} else if (enemy_aircraft_count > 0 && anti_air_count < MAX(1, enemy_aircraft_count / 2)) {
			if (Can_Build(&BuildingTypeClass::As_Reference(STRUCT_SAM), ActLike)) {
				reserve_build = STRUCT_SAM;
			} else if ((flags & STRUCTF_EYE) && Can_Build(&BuildingTypeClass::As_Reference(STRUCT_ATOWER), ActLike)) {
				reserve_build = STRUCT_ATOWER;
			}
		} else if (!(flags & STRUCTF_REPAIR) &&
			((flags & (STRUCTF_WEAP | STRUCTF_AIRSTRIP)) || combat_units >= 4)) {
			reserve_build = STRUCT_REPAIR;
		} else if ((personality == 2 || enemy_aircraft_count > 0) && !(flags & STRUCTF_HELIPAD) &&
			Can_Build(&BuildingTypeClass::As_Reference(STRUCT_HELIPAD), ActLike)) {
			reserve_build = STRUCT_HELIPAD;
		} else if (flags & STRUCTF_RADAR) {
			desired_defense_count = MAX(1, (valuable_count + 1) / 2);
			if (under_pressure) {
				desired_defense_count++;
				if (big_map) {
					desired_defense_count++;
				}
			}
			if (economy_harassed) {
				desired_defense_count++;
			}
			if (defense_count < desired_defense_count) {
				if (ActLike == HOUSE_GOOD) {
					if ((flags & STRUCTF_EYE) && !(flags & STRUCTF_ATOWER) &&
						Can_Build(&BuildingTypeClass::As_Reference(STRUCT_ATOWER), ActLike)) {
						reserve_build = STRUCT_ATOWER;
					} else {
						reserve_build = STRUCT_GTOWER;
					}
				} else {
					if ((flags & STRUCTF_TEMPLE) && !(flags & STRUCTF_OBELISK) &&
						Can_Build(&BuildingTypeClass::As_Reference(STRUCT_OBELISK), ActLike)) {
						reserve_build = STRUCT_OBELISK;
					} else {
						reserve_build = STRUCT_TURRET;
					}
				}
			}
		} else if (ActLike == HOUSE_GOOD && !(flags & STRUCTF_EYE)) {
			reserve_build = STRUCT_EYE;
		} else if (ActLike == HOUSE_BAD && !(flags & STRUCTF_TEMPLE)) {
			reserve_build = STRUCT_TEMPLE;
		} else if ((flags & STRUCTF_RADAR) && defense_count < MAX(1, (valuable_count + 1) / 2)) {
			if (ActLike == HOUSE_GOOD) {
				if ((flags & STRUCTF_EYE) && !(flags & STRUCTF_ATOWER) &&
					Can_Build(&BuildingTypeClass::As_Reference(STRUCT_ATOWER), ActLike)) {
					reserve_build = STRUCT_ATOWER;
				} else {
					reserve_build = STRUCT_GTOWER;
				}
			} else {
				if ((flags & STRUCTF_TEMPLE) && !(flags & STRUCTF_OBELISK) &&
					Can_Build(&BuildingTypeClass::As_Reference(STRUCT_OBELISK), ActLike)) {
					reserve_build = STRUCT_OBELISK;
				} else {
					reserve_build = STRUCT_TURRET;
				}
			}
		} else if (!(flags & STRUCTF_REPAIR)) {
			reserve_build = STRUCT_REPAIR;
		} else if ((Power - Drain) < 75 && !(flags & STRUCTF_ADVANCED_POWER)) {
			reserve_build = STRUCT_ADVANCED_POWER;
		}

		if (reserve_build != STRUCT_NONE) {
			BuildingTypeClass const & buildtype = BuildingTypeClass::As_Reference(reserve_build);

			if (Can_Build(&buildtype, ActLike)) {
				reserve = buildtype.Cost_Of();
			} else {
				reserve_build = STRUCT_NONE;
			}
		}
	}

	switch (objecttype) {

		/*
		**	Unit construction is based on the rule that up to twice the number required
		**	to fill all teams will be created.
		*/
		case RTTI_UNIT:
		case RTTI_UNITTYPE:
			if (GameToPlay == GAME_SKIRMISH || CurUnits < MaxUnit) {

				/*
				**	A computer controlled house will try to build a replacement
				**	harvester if possible. Never replace harvesters if the game
				**	is in easy mode.
				*/
				if (GameToPlay != GAME_SKIRMISH && !Special.IsEasy && !IsHuman && (ActiveBScan & STRUCTF_REFINERY) && !(UScan & UNITF_HARVESTER)) {
					techno = &UnitTypeClass::As_Reference(UNIT_HARVESTER);
					if (techno->Scenario <= BuildLevel) break;
					techno = 0;
				}

				/*
				**	Skirmish AI uses a simpler roster-driven production rule. The
				**	legacy multiplayer path desires every buildable unit at once,
				**	which is too spammy for local skirmish.
				*/
				if (GameToPlay == GAME_SKIRMISH && !IsHuman) {
					static UnitType const gdi_units[] = {
						UNIT_JEEP,
						UNIT_MTANK,
						UNIT_APC,
						UNIT_MLRS,
						UNIT_HTANK
					};
					static UnitType const nod_units[] = {
						UNIT_BUGGY,
						UNIT_BIKE,
						UNIT_LTANK,
						UNIT_ARTY,
						UNIT_STANK,
						UNIT_FTANK
					};
					UnitType const * list = (ActLike == HOUSE_BAD) ? nod_units : gdi_units;
					int list_count = (ActLike == HOUSE_BAD) ? (sizeof(nod_units) / sizeof(nod_units[0])) : (sizeof(gdi_units) / sizeof(gdi_units[0]));
					int bestcount = 0;
					UnitType bestlist[8];
					long money = Available_Money();
					int personality = Skirmish_AI_Personality(this);
					int refinery_count = 0;
					int harvester_count = 0;
					int enemy_infantry = 0;
					int enemy_units = 0;
					int enemy_buildings = 0;
					int minimum_force = Skirmish_AI_Min_Combat_Force(MPlayerAISkill, personality);
					bool hard_reserve = Skirmish_AI_Core_Reserve(reserve_build);
					bool late_game = ((flags & STRUCTF_RADAR) &&
						((flags & STRUCTF_REPAIR) || (flags & STRUCTF_EYE) || (flags & STRUCTF_TEMPLE) ||
						 (flags & STRUCTF_ADVANCED_POWER)));

					if (recovery_mode && reserve_build != STRUCT_NONE && (hard_reserve || combat_units >= minimum_force)) {
						break;
					}

					if (economy_harassed && reserve_build != STRUCT_NONE && (hard_reserve || combat_units >= minimum_force)) {
						break;
					}

					if (reserve && money < reserve + 600 && (hard_reserve || combat_units >= minimum_force)) {
						break;
					}

					Skirmish_AI_Enemy_Mix(this, enemy_infantry, enemy_units, enemy_buildings);

					for (int idx = 0; idx < Buildings.Count(); idx++) {
						BuildingClass * building = Buildings.Ptr(idx);

						if (building && !building->IsInLimbo && building->House == this && building->Strength > 0 &&
							*building == STRUCT_REFINERY) {
							refinery_count++;
						}
					}

					for (int idx = 0; idx < Units.Count(); idx++) {
						UnitClass * unit = Units.Ptr(idx);

						if (unit && !unit->IsInLimbo && unit->House == this && unit->Strength > 0 &&
							*unit == UNIT_HARVESTER) {
							harvester_count++;
						}
					}

					if ((refinery_count >= 2 && harvester_count < refinery_count) ||
						(refinery_count >= 1 && (flags & (STRUCTF_WEAP | STRUCTF_AIRSTRIP)) && harvester_count < 2) ||
						(economy_harassed && refinery_count >= 1 && harvester_count < MAX(2, refinery_count))) {
						if (Can_Build(UNIT_HARVESTER, ActLike) &&
							UnitTypeClass::As_Reference(UNIT_HARVESTER).Cost_Of() <= money &&
							(!reserve || (money - UnitTypeClass::As_Reference(UNIT_HARVESTER).Cost_Of()) >= reserve)) {
							techno = &UnitTypeClass::As_Reference(UNIT_HARVESTER);
							break;
						}
					}

					for (int idx = 0; idx < list_count; idx++) {
						UnitType utype = list[idx];
						bool prefer = true;

						switch (personality) {
							default:
							case 0:
								prefer = (ActLike == HOUSE_BAD) ? (utype == UNIT_BUGGY || utype == UNIT_BIKE || utype == UNIT_FTANK)
									: (utype == UNIT_JEEP || utype == UNIT_APC);
								break;

							case 1:
								prefer = (ActLike == HOUSE_BAD) ? (utype == UNIT_LTANK || utype == UNIT_ARTY || utype == UNIT_STANK)
									: (utype == UNIT_MTANK || utype == UNIT_MLRS || utype == UNIT_HTANK);
								break;

							case 2:
								prefer = (ActLike == HOUSE_BAD) ? (utype == UNIT_BIKE || utype == UNIT_BUGGY || utype == UNIT_STANK)
									: (utype == UNIT_MLRS || utype == UNIT_APC || utype == UNIT_JEEP);
								break;
						}

						if (late_game) {
							if (ActLike == HOUSE_BAD) {
								if (utype == UNIT_STANK || utype == UNIT_FTANK || utype == UNIT_ARTY) {
									prefer = true;
								}
							} else {
								if (utype == UNIT_HTANK || utype == UNIT_MLRS) {
									prefer = true;
								}
							}
						}

						if (enemy_infantry > enemy_units + 2) {
							if (utype == UNIT_BUGGY || utype == UNIT_FTANK || utype == UNIT_JEEP || utype == UNIT_APC) {
								prefer = true;
							}
						}
						if (enemy_units > enemy_infantry + 1) {
							if (utype == UNIT_MTANK || utype == UNIT_HTANK || utype == UNIT_LTANK || utype == UNIT_STANK ||
								utype == UNIT_BIKE || utype == UNIT_MLRS || utype == UNIT_ARTY) {
								prefer = true;
							}
						}
						if (enemy_buildings >= MAX(3, enemy_units) &&
							(utype == UNIT_MLRS || utype == UNIT_ARTY || utype == UNIT_HTANK || utype == UNIT_STANK)) {
							prefer = true;
						}

						if (Can_Build(utype, ActLike) &&
							UnitTypeClass::As_Reference(utype).Level <= BuildLevel &&
							UnitTypeClass::As_Reference(utype).Cost_Of() <= money &&
							(!reserve || (money - UnitTypeClass::As_Reference(utype).Cost_Of()) >= reserve) &&
							(prefer || !bestcount)) {
							bestlist[bestcount++] = utype;
						}
					}

					if (bestcount) {
						techno = &UnitTypeClass::As_Reference(bestlist[Random_Pick(0, bestcount-1)]);
					}
					break;
				}

				int counter[UNIT_COUNT];
				if (GameToPlay == GAME_NORMAL) {
					memset(counter, 0x00, sizeof(counter));
				} else {
					for (UnitType index = UNIT_FIRST; index < UNIT_COUNT; index++) {
						if (Can_Build(index, Class->House) && UnitTypeClass::As_Reference(index).Level <= BuildLevel) {
							counter[index] = 16;
						} else {
							counter[index] = 0;
						}
					}
				}

				/*
				**	Build a list of the maximum of each type we wish to produce. This will be
				**	twice the number required to fill all teams.
				*/
					for (int index = 0; index < Teams.Count(); index++) {
					TeamClass * tptr = Teams.Ptr(index);
					if (tptr) {
						TeamTypeClass const * team = tptr->Class;

						if ((/*team->IsReinforcable || */!tptr->IsFullStrength) && team->House == Class->House) {
							for (int subindex = 0; subindex < team->ClassCount; subindex++) {
								if (team->Class[subindex]->What_Am_I() == RTTI_UNITTYPE) {
									counter[((UnitTypeClass const *)(team->Class[subindex]))->Type] = 1;
//									counter[((UnitTypeClass const *)(team->Class[subindex]))->Type] += team->DesiredNum[subindex]*2;
								}
							}
						}
					}
				}

				/*
				**	Team types that are flagged as prebuilt, will always try to produce enough
				**	to fill one team of this type regardless of whether there is a team active
				**	of that type.
				*/
				for (int index = 0; index < TeamTypes.Count(); index++) {
					TeamTypeClass const * team = TeamTypes.Ptr(index);
					if (team) {
						if (team->House == Class->House && team->IsPrebuilt && (!team->IsAutocreate || IsAlerted)) {
							for (int subindex = 0; subindex < team->ClassCount; subindex++) {
								if (team->Class[subindex]->What_Am_I() == RTTI_UNITTYPE) {
									int subtype = ((UnitTypeClass const *)(team->Class[subindex]))->Type;
									counter[subtype] = MAX(counter[subtype], team->DesiredNum[subindex]);
								}
							}
						}
					}
				}

				/*
				**	Reduce the theoretical maximum by the actual number of objects currently
				**	in play.
				*/
				for (int uindex = 0; uindex < Units.Count(); uindex++) {
					UnitClass * unit = Units.Ptr(uindex);
					if (unit && !unit->Team && unit->House == this && (unit->Mission != MISSION_GUARD_AREA && unit->Mission != MISSION_HUNT && unit->Mission != MISSION_STICKY && unit->Mission != MISSION_SLEEP)) {
						counter[unit->Class->Type]--;
					}
				}

				/*
				**	Pick to build the most needed object but don't consider those object that
				**	can't be built because of scenario restrictions or insufficient cash.
				*/
				int bestval = -1;
				int bestcount = 0;
				UnitType bestlist[UNIT_COUNT];
				for (UnitType utype = UNIT_FIRST; utype < UNIT_COUNT; utype++) {
					if (counter[utype] > 0 && Can_Build(utype, Class->House) && UnitTypeClass::As_Reference(utype).Cost_Of() <= Available_Money()) {
						if (bestval == -1 || bestval < counter[utype]) {
							bestval = counter[utype];
							bestcount = 0;
						}
						bestlist[bestcount++] = utype;
					}
				}

				/*
				**	The unit type to build is now known. Fetch a pointer to the techno type class.
				*/
				if (bestcount) {
					techno = &UnitTypeClass::As_Reference(bestlist[Random_Pick(0, bestcount-1)]);
				}
			}
			break;

		/*
		**	Infantry construction is based on the rule that up to twice the number required
		**	to fill all teams will be created.
		*/
		case RTTI_INFANTRY:
		case RTTI_INFANTRYTYPE:
			if (GameToPlay == GAME_SKIRMISH || CurUnits < MaxUnit) {
				if (GameToPlay == GAME_SKIRMISH && !IsHuman) {
					static InfantryType const gdi_infantry[] = {
						INFANTRY_E1,
						INFANTRY_E2,
						INFANTRY_E3,
						INFANTRY_E5
					};
					static InfantryType const nod_infantry[] = {
						INFANTRY_E1,
						INFANTRY_E3,
						INFANTRY_E4,
						INFANTRY_E5
					};
					InfantryType const * list = (ActLike == HOUSE_BAD) ? nod_infantry : gdi_infantry;
					int list_count = (ActLike == HOUSE_BAD) ? (sizeof(nod_infantry) / sizeof(nod_infantry[0])) : (sizeof(gdi_infantry) / sizeof(gdi_infantry[0]));
					int bestcount = 0;
					InfantryType bestlist[8];
					long money = Available_Money();
					int unit_goal = 4;
					int personality = Skirmish_AI_Personality(this);
					int enemy_infantry = 0;
					int enemy_units = 0;
					int enemy_buildings = 0;
					int minimum_force = Skirmish_AI_Min_Combat_Force(MPlayerAISkill, personality);
					bool hard_reserve = Skirmish_AI_Core_Reserve(reserve_build);
					bool late_game = ((flags & STRUCTF_RADAR) &&
						((flags & STRUCTF_REPAIR) || (flags & STRUCTF_EYE) || (flags & STRUCTF_TEMPLE) ||
						 (flags & STRUCTF_ADVANCED_POWER)));

					switch (MPlayerAISkill) {
						default:
						case 0:
							unit_goal = 5;
							break;

						case 1:
							unit_goal = 4;
							break;

						case 2:
							unit_goal = 3;
							break;
					}

					if (personality == 0) {
						unit_goal = MAX(1, unit_goal - 2);
					} else if (personality == 1 || personality == 2) {
						unit_goal += 1;
					}

					if ((flags & (STRUCTF_WEAP | STRUCTF_AIRSTRIP)) && combat_units < unit_goal) {
						break;
					}

					if (recovery_mode && reserve_build != STRUCT_NONE && (hard_reserve || combat_units >= minimum_force)) {
						break;
					}

					if (economy_harassed && reserve_build != STRUCT_NONE && (hard_reserve || combat_units >= minimum_force)) {
						break;
					}

					if (reserve && money < reserve + 400 && (hard_reserve || combat_units >= minimum_force)) {
						break;
					}

					Skirmish_AI_Enemy_Mix(this, enemy_infantry, enemy_units, enemy_buildings);

					for (int idx = 0; idx < list_count; idx++) {
						InfantryType utype = list[idx];
						bool prefer = true;

						switch (personality) {
							default:
							case 0:
								prefer = (utype == INFANTRY_E1 || utype == INFANTRY_E3 || utype == INFANTRY_E4);
								break;

							case 1:
								prefer = (utype == INFANTRY_E2 || utype == INFANTRY_E3 || utype == INFANTRY_E5);
								break;

							case 2:
								prefer = (utype == INFANTRY_E2 || utype == INFANTRY_E5);
								break;
						}

						if (late_game && (utype == INFANTRY_E3 || utype == INFANTRY_E5)) {
							prefer = true;
						}

						if (enemy_infantry > enemy_units + 2 && (utype == INFANTRY_E2 || utype == INFANTRY_E4)) {
							prefer = true;
						}
						if ((enemy_units > enemy_infantry + 1 || enemy_buildings > enemy_infantry + 2) &&
							(utype == INFANTRY_E3 || utype == INFANTRY_E5)) {
							prefer = true;
						}

						if (Can_Build(utype, ActLike) &&
							InfantryTypeClass::As_Reference(utype).Level <= BuildLevel &&
							InfantryTypeClass::As_Reference(utype).Cost_Of() <= money &&
							(!reserve || (money - InfantryTypeClass::As_Reference(utype).Cost_Of()) >= reserve) &&
							(prefer || !bestcount)) {
							bestlist[bestcount++] = utype;
						}
					}

					if (bestcount) {
						techno = &InfantryTypeClass::As_Reference(bestlist[Random_Pick(0, bestcount-1)]);
					}
					break;
				}

				int counter[INFANTRY_COUNT];
				if (GameToPlay == GAME_NORMAL) {
					memset(counter, 0x00, sizeof(counter));
				} else {
					for (InfantryType index = INFANTRY_FIRST; index < INFANTRY_COUNT; index++) {
						if (Can_Build(index, Class->House) && InfantryTypeClass::As_Reference(index).Level <= BuildLevel) {
							counter[index] = 16;
						} else {
							counter[index] = 0;
						}
					}
				}

				/*
				**	Build a list of the maximum of each type we wish to produce. This will be
				**	twice the number required to fill all teams.
				*/
				for (int index = 0; index < Teams.Count(); index++) {
					TeamClass * tptr = Teams.Ptr(index);
					if (tptr) {
						TeamTypeClass const * team = tptr->Class;

						if ((team->IsReinforcable || !tptr->IsFullStrength) && team->House == Class->House) {
							for (int subindex = 0; subindex < team->ClassCount; subindex++) {
								if (team->Class[subindex]->What_Am_I() == RTTI_INFANTRYTYPE) {
									counter[((InfantryTypeClass const *)(team->Class[subindex]))->Type] += team->DesiredNum[subindex]+1;
								}
							}
						}
					}
				}

				/*
				**	Team types that are flagged as prebuilt, will always try to produce enough
				**	to fill one team of this type regardless of whether there is a team active
				**	of that type.
				*/
				for (int index = 0; index < TeamTypes.Count(); index++) {
					TeamTypeClass const * team = TeamTypes.Ptr(index);
					if (team) {
						if (team->House == Class->House && team->IsPrebuilt && (!team->IsAutocreate || IsAlerted)) {
							for (int subindex = 0; subindex < team->ClassCount; subindex++) {
								if (team->Class[subindex]->What_Am_I() == RTTI_INFANTRYTYPE) {
									int subtype = ((InfantryTypeClass const *)(team->Class[subindex]))->Type;
//									counter[subtype] = 1;
									counter[subtype] = MAX(counter[subtype], team->DesiredNum[subindex]);
									counter[subtype] = MIN(counter[subtype], 5);
								}
							}
						}
					}
				}

				/*
				**	Reduce the theoretical maximum by the actual number of objects currently
				**	in play.
				*/
				for (int uindex = 0; uindex < Infantry.Count(); uindex++) {
					InfantryClass * infantry = Infantry.Ptr(uindex);
					if (infantry && !infantry->Team && infantry->House == this && (infantry->Mission != MISSION_GUARD_AREA && infantry->Mission != MISSION_HUNT && infantry->Mission != MISSION_STICKY && infantry->Mission != MISSION_SLEEP)) {
						counter[infantry->Class->Type]--;
					}
				}

				/*
				**	Pick to build the most needed object but don't consider those object that
				**	can't be built because of scenario restrictions or insufficient cash.
				*/
				int bestval = -1;
				int bestcount = 0;
				InfantryType bestlist[INFANTRY_COUNT];
				for (InfantryType utype = INFANTRY_FIRST; utype < INFANTRY_COUNT; utype++) {
					if (counter[utype] > 0 && Can_Build(utype, Class->House) && InfantryTypeClass::As_Reference(utype).Cost_Of() <= Available_Money()) {
						if (bestval == -1 || bestval < counter[utype]) {
							bestval = counter[utype];
							bestcount = 0;
						}
						bestlist[bestcount++] = utype;
					}
				}

				/*
				**	The infantry type to build is now known. Fetch a pointer to the techno type class.
				*/
				if (bestcount) {
					techno = &InfantryTypeClass::As_Reference(bestlist[Random_Pick(0, bestcount-1)]);
				}
			}
			break;

		case RTTI_AIRCRAFT:
		case RTTI_AIRCRAFTTYPE:
			if (GameToPlay == GAME_SKIRMISH && !IsHuman) {
				AircraftType atype = (ActLike == HOUSE_GOOD) ? AIRCRAFT_ORCA : AIRCRAFT_HELICOPTER;
				long money = Available_Money();
				int desired_aircraft = 0;
				int minimum_force = Skirmish_AI_Min_Combat_Force(MPlayerAISkill, personality);
				bool hard_reserve = Skirmish_AI_Core_Reserve(reserve_build);
				bool late_game = ((flags & STRUCTF_RADAR) &&
					((flags & STRUCTF_REPAIR) || (flags & STRUCTF_EYE) || (flags & STRUCTF_TEMPLE) ||
					 (flags & STRUCTF_ADVANCED_POWER)));

				if (recovery_mode && reserve_build != STRUCT_NONE && (hard_reserve || combat_units >= minimum_force)) {
					break;
				}

				if (economy_harassed) {
					break;
				}

				if (!(flags & STRUCTF_HELIPAD)) {
					break;
				}
				if (Skirmish_AI_Personality(this) == 2) {
					desired_aircraft = MAX(desired_aircraft, helipad_count);
				}
				if (enemy_aircraft_count > 0) {
					desired_aircraft = MAX(desired_aircraft, MIN(helipad_count, enemy_aircraft_count));
				}
				if (late_game) {
					desired_aircraft = MAX(desired_aircraft, helipad_count);
				}
				if (desired_aircraft <= combat_aircraft_count) {
					break;
				}
				if (reserve && money < reserve + 800 && (hard_reserve || combat_units >= minimum_force)) {
					break;
				}
				if (Can_Build(atype, ActLike) &&
					AircraftTypeClass::As_Reference(atype).Level <= BuildLevel &&
					AircraftTypeClass::As_Reference(atype).Cost_Of() <= money &&
					(!reserve || (money - AircraftTypeClass::As_Reference(atype).Cost_Of()) >= reserve)) {
					techno = &AircraftTypeClass::As_Reference(atype);
				}
			}
			break;

		/*
		**	Building construction is based upon the preconstruction list.
		*/
		case RTTI_BUILDING:
		case RTTI_BUILDINGTYPE:
			if (CurBuildings < MaxBuilding) {
				BaseNodeClass * node = Base.Next_Buildable();
				if (node) {
					BuildingTypeClass const & buildtype = BuildingTypeClass::As_Reference(node->Type);
					if (GameToPlay != GAME_SKIRMISH || IsHuman) {
						techno = &buildtype;
					} else {
						if (Can_Build(&buildtype, ActLike) && buildtype.Cost_Of() <= Available_Money()) {
							techno = &buildtype;
						}
					}
				} else if (GameToPlay == GAME_SKIRMISH && !IsHuman) {
					StructType build = STRUCT_NONE;
					bool late_game = false;
					bool wants_air = false;

					if (flags & STRUCTF_ADVANCED_POWER) flags |= STRUCTF_POWER;
					if (flags & STRUCTF_HAND) flags |= STRUCTF_BARRACKS;
					if (flags & STRUCTF_AIRSTRIP) flags |= STRUCTF_WEAP;
					if (flags & STRUCTF_OBELISK) flags |= STRUCTF_ATOWER;

					late_game = ((flags & STRUCTF_RADAR) &&
						((flags & STRUCTF_REPAIR) || (flags & STRUCTF_EYE) || (flags & STRUCTF_TEMPLE) ||
						 (flags & STRUCTF_ATOWER) || (flags & STRUCTF_OBELISK) || (flags & STRUCTF_ADVANCED_POWER)));
					wants_air = (personality == 2 || enemy_aircraft_count > 0);

					if (!(flags & STRUCTF_POWER)) {
						build = STRUCT_POWER;
					} else if (!(flags & STRUCTF_REFINERY)) {
						build = STRUCT_REFINERY;
					} else if (ActLike == HOUSE_GOOD && !(flags & STRUCTF_BARRACKS)) {
						build = STRUCT_BARRACKS;
					} else if (ActLike == HOUSE_BAD && !(flags & STRUCTF_HAND)) {
						build = STRUCT_HAND;
					} else if (ActLike == HOUSE_GOOD && !(flags & STRUCTF_WEAP)) {
						build = STRUCT_WEAP;
					} else if (ActLike == HOUSE_BAD && !(flags & STRUCTF_AIRSTRIP)) {
						build = STRUCT_AIRSTRIP;
					} else if (ActLike == HOUSE_GOOD && !(flags & STRUCTF_RADAR)) {
						build = STRUCT_RADAR;
					} else if (!(flags & STRUCTF_REPAIR) &&
						((flags & (STRUCTF_WEAP | STRUCTF_AIRSTRIP)) || combat_units >= 4)) {
						build = STRUCT_REPAIR;
					} else if (wants_air && !(flags & STRUCTF_HELIPAD) &&
						Can_Build(&BuildingTypeClass::As_Reference(STRUCT_HELIPAD), ActLike)) {
						build = STRUCT_HELIPAD;
					} else if (ActLike == HOUSE_GOOD && !(flags & STRUCTF_EYE)) {
						build = STRUCT_EYE;
					} else if (ActLike == HOUSE_BAD && !(flags & STRUCTF_TEMPLE)) {
						build = STRUCT_TEMPLE;
					} else if ((Power - Drain) < 75 && !(flags & STRUCTF_ADVANCED_POWER) &&
						Can_Build(&BuildingTypeClass::As_Reference(STRUCT_ADVANCED_POWER), ActLike)) {
						build = STRUCT_ADVANCED_POWER;
					} else if (reserve_build != STRUCT_NONE) {
						build = reserve_build;
					} else if (late_game && ActLike == HOUSE_GOOD && !(flags & STRUCTF_ATOWER) &&
						(flags & STRUCTF_EYE) && Can_Build(&BuildingTypeClass::As_Reference(STRUCT_ATOWER), ActLike)) {
						build = STRUCT_ATOWER;
					} else if (late_game && ActLike == HOUSE_BAD && !(flags & STRUCTF_OBELISK) &&
						(flags & STRUCTF_TEMPLE) && Can_Build(&BuildingTypeClass::As_Reference(STRUCT_OBELISK), ActLike)) {
						build = STRUCT_OBELISK;
					} else if (Power <= Drain) {
						build = STRUCT_POWER;
					} else if ((Tiberium / 2) > Capacity) {
						build = STRUCT_REFINERY;
					}

					if (build != STRUCT_NONE &&
						Can_Build(&BuildingTypeClass::As_Reference(build), ActLike) &&
						BuildingTypeClass::As_Reference(build).Cost_Of() <= Available_Money()) {
						techno = &BuildingTypeClass::As_Reference(build);
					}
				}
			}
			break;
	}
	return(techno);
}


/***********************************************************************************************
 * HouseClass::Flag_Remove -- Removes the flag from the specified target.                      *
 *                                                                                             *
 *    This routine will remove the flag attached to the specified target object or cell.       *
 *    Call this routine before placing the object down. This is called inherently by the       *
 *    the Flag_Attach() functions.                                                             *
 *                                                                                             *
 * INPUT:   target   -- The target that the flag was attached to but will be removed from.     *
 *                                                                                             *
 *          set_home -- if true, clears the flag's waypoint designation                        *
 *                                                                                             *
 * OUTPUT:  Was the flag successfully removed from the specified target?                       *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Flag_Remove(TARGET target, bool set_home)
{
	Validate();
	int rc;

	if (Target_Legal(target)) {

		/*
		**	Remove the flag from a unit
		*/
		UnitClass * object = As_Unit(target);
		if (object) {
			rc = object->Flag_Remove();
			if (rc && FlagLocation == target) {
				FlagLocation = TARGET_NONE;
			}

		} else {

			/*
			**	Remove the flag from a cell
			*/
			CELL cell = As_Cell(target);
			if (Map.In_Radar(cell)) {
				rc = Map[cell].Flag_Remove();
				if (rc && FlagLocation == target) {
					FlagLocation = TARGET_NONE;
				}
			}
		}

		/*
		**	Handle the flag home cell:
		**	If 'set_home' is set, clear the home value & the cell's overlay
		*/
		if (set_home) {
			if (FlagHome) {
				Map[FlagHome].Overlay = OVERLAY_NONE;
				Map.Flag_Cell(FlagHome);
				FlagHome = 0;
			}
		}

		return(rc);
	}
	return(false);
}


/***********************************************************************************************
 * HouseClass::Flag_Attach -- Attach flag to specified cell (or thereabouts).                  *
 *                                                                                             *
 *    This routine will attach the house flag to the location specified. If the location       *
 *    cannot contain the flag, then a suitable nearby location will be selected.               *
 *                                                                                             *
 * INPUT:   cell  -- The desired cell location to place the flag.                              *
 *                                                                                             *
 *          set_home -- if true, resets the flag's waypoint designation                        *
 *                                                                                             *
 * OUTPUT:  Was the flag successfully placed?                                                  *
 *                                                                                             *
 * WARNINGS:   The cell picked for the flag might very likely not be the cell requested.       *
 *             Check the FlagLocation value to determine the final cell resting spot.          *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Flag_Attach(CELL cell, bool set_home)
{
	Validate();
	bool rc;
	bool clockwise;
	FacingType rot;
	FacingType fcounter;

	/*
	**	Randomly decide if we're going to search cells clockwise or counter-
	**	clockwise
	*/
	clockwise = IRandom(0,1);

	/*
	**	Only continue if this cell is a legal placement cell.
	*/
	if (Map.In_Radar(cell)) {

		/*
		**	If the flag already exists, then it must be removed from the object
		**	it is attached to.
		*/
		Flag_Remove(FlagLocation, set_home);

		/*
		**	Attach the flag to the cell specified. If it can't be placed, then pick
		**	a nearby cell where it can be placed.
		*/
		CELL newcell = cell;
		rc = Map[newcell].Flag_Place(Class->House);
		if (!rc) {

			/*
			**	Loop for increasing distance from the desired cell.
			**	For each distance, randomly pick a starting direction.  Between
			**	this and the clockwise/counterclockwise random value, the flag
			**	should appear to be placed fairly randomly.
			*/
			for (int dist = 1; dist < 32; dist++) {

				/*
				**	Clockwise search.
				*/
				if (clockwise) {
					rot = (FacingType)IRandom(FACING_N, FACING_NW);
					for (fcounter = FACING_N; fcounter <= FACING_NW; fcounter++) {
						newcell = Coord_Cell(Coord_Move(Cell_Coord(cell), Facing_Dir(rot), dist*256));
						if (Map.In_Radar(newcell) && Map[newcell].Flag_Place(Class->House)) {
							dist = 32;
							rc = true;
							break;
						}
						rot++;
						if (rot > FACING_NW) rot = FACING_N;
					}
				} else {

					/*
					**	Counter-clockwise search
					*/
					rot = (FacingType)IRandom (FACING_N, FACING_NW);
					for (fcounter = FACING_NW; fcounter >= FACING_N; fcounter--) {
						newcell = Coord_Cell(Coord_Move(Cell_Coord(cell), Facing_Dir(rot), dist*256));
						if (Map.In_Radar(newcell) && Map[newcell].Flag_Place(Class->House)) {
							dist = 32;
							rc = true;
							break;
						}
						rot--;
						if (rot < FACING_N)
							rot = FACING_NW;
					}
				}
			}
		}

		/*
		**	If we've found a spot for the flag, place the flag at the new cell.
		**	if 'set_home' is set, OR this house has no current flag home cell,
		**	mark that cell as this house's flag home cell.
		*/
		if (rc) {
			FlagLocation = As_Target(newcell);

			if (set_home || FlagHome == 0) {
				Map[newcell].Overlay = OVERLAY_FLAG_SPOT;
				FlagHome = newcell;
			}
		}

		return(rc);
	}
	return(false);
}


/***********************************************************************************************
 * HouseClass::Flag_Attach -- Attaches the house flag the specified unit.                      *
 *                                                                                             *
 *    This routine will attach the house flag to the specified unit. This routine is called    *
 *    when a unit drives over a cell containing a flag.                                        *
 *                                                                                             *
 * INPUT:   object   -- Pointer to the object that the house flag is to be attached to.        *
 *                                                                                             *
 *          set_home -- if true, clears the flag's waypoint designation                        *
 *                                                                                             *
 * OUTPUT:  Was the flag attached successfully?                                                *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   05/23/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Flag_Attach(UnitClass * object, bool set_home)
{
	Validate();
	if (object && !object->IsInLimbo) {
		Flag_Remove(FlagLocation, set_home);

		/*
		**	Attach the flag to the object.
		*/
		object->Flag_Attach(Class->House);
		FlagLocation = object->As_Target();
		return(true);
	}
	return(false);
}


/***************************************************************************
 * HouseClass::MPlayer_Defeated -- multiplayer; house is defeated          *
 *                                                                         *
 * INPUT:                                                                  *
 *      none.                                                              *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      none.                                                              *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/25/1995 BRR : Created.                                             *
 *=========================================================================*/
void HouseClass::MPlayer_Defeated(void)
{
	Validate();
	char txt[80];
	int i,j,k;
	unsigned char id;
	HousesType house;
	HouseClass *hptr;
	HouseClass *hptr2;
	int num_alive;
	int num_humans;
	int all_allies;
	int max_index;
	int max_count;
	int count;
	int score_index[MAX_PLAYERS];	// array of each multi-player's index into
											// the score array

	/*------------------------------------------------------------------------
	Set the defeat flag for this house
	------------------------------------------------------------------------*/
	IsDefeated = true;

	/*------------------------------------------------------------------------
	Remove this house's flag & flag home cell
	------------------------------------------------------------------------*/
	if (Special.IsCaptureTheFlag) {
		if (FlagLocation) {
			Flag_Remove(FlagLocation,true);
		} else {
			if (FlagHome) {
				Flag_Remove(FlagHome,true);
			}
		}
	}

	/*------------------------------------------------------------------------
	If this is me:
	- Set MPlayerObiWan, so I can only send messages to all players, and
	  not just one (so I can't be obnoxiously omnipotent)
	- Reveal the map
	- Add my defeat message
	------------------------------------------------------------------------*/
	if (PlayerPtr == this) {
		MPlayerObiWan = 1;
		Debug_Unshroud = true;
		HiddenPage.Clear();
		Map.Flag_To_Redraw(true);

		/*.....................................................................
		Pop up a message showing that I was defeated
		.....................................................................*/
		sprintf(txt,Text_String(TXT_PLAYER_DEFEATED), MPlayerName);
		Messages.Add_Message(txt, MPlayerTColors[MPlayerColorIdx], TPF_6PT_GRAD|TPF_USE_GRAD_PAL|TPF_FULLSHADOW, 600, 0, 0);
		Map.Flag_To_Redraw(false);

	} else {

		/*------------------------------------------------------------------------
		If it wasn't me, find out who was defeated
		------------------------------------------------------------------------*/
		if (IsHuman) {
			sprintf(txt, Text_String(TXT_PLAYER_DEFEATED), Text_String(TXT_UNKNOWN));
			id = 0;
			for (i = 0; i < MPlayerCount; i++) {
				house = MPlayerHouses[i];
				if (HouseClass::As_Pointer(house) == this) {
					sprintf (txt,Text_String(TXT_PLAYER_DEFEATED), MPlayerNames[i]);
					id = MPlayerID[i];
				}
			}

			Messages.Add_Message(txt, MPlayerTColors[MPlayerID_To_ColorIndex(id)],
				TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_FULLSHADOW, 600, 0, 0);
			Map.Flag_To_Redraw(false);
		}
	}

	/*------------------------------------------------------------------------
	Find out how many players are left alive.
	------------------------------------------------------------------------*/
	num_alive = 0;
	num_humans = 0;
	for (i = 0; i < MPlayerMax; i++) {
		hptr = HouseClass::As_Pointer((HousesType)(HOUSE_MULTI1 + i));
		if (hptr && hptr->IsDefeated==0) {
			if (hptr->IsHuman)
				num_humans++;
			num_alive++;
		}
	}

	/*------------------------------------------------------------------------
	If all the houses left alive are allied with each other, then in reality
	there's only one player left:
	------------------------------------------------------------------------*/
	all_allies = 1;
	for (i = 0; i < MPlayerMax; i++) {
		/*.....................................................................
		Get a pointer to this house
		.....................................................................*/
		hptr = HouseClass::As_Pointer((HousesType)(HOUSE_MULTI1 + i));
		if (!hptr || hptr->IsDefeated)
			continue;

		/*.....................................................................
		Loop through all houses; if there's one left alive that this house
		isn't allied with, then all_allies will be false
		.....................................................................*/
		for (j = 0; j < MPlayerMax; j++) {
			hptr2 = HouseClass::As_Pointer((HousesType)(HOUSE_MULTI1 + j));
			if (!hptr2)
				continue;
			if (!hptr2->IsDefeated && !hptr->Is_Ally(hptr2)) {
				all_allies = 0;
				break;
			}
		}
		if (!all_allies)
			break;
	}
	/*........................................................................
	If all houses left are allies, set 'num_alive' to 1; game over.
	........................................................................*/
	if (all_allies)
		num_alive = 1;

	/*------------------------------------------------------------------------
	If there's only one human player left or no humans left, the game is over:
	- Determine whether this player wins or loses, based on the state of the
	  MPlayerObiWan flag
	- Find all players' indices in the MPlayerScore array
	- Tally up scores for this game
	------------------------------------------------------------------------*/
	if (num_alive == 1 || num_humans == 0) {
		if (PlayerPtr->IsDefeated) {
			PlayerLoses = true;
		} else {
			PlayerWins = true;
		}

		/*---------------------------------------------------------------------
		Find each player's score index
		---------------------------------------------------------------------*/
		for (i = 0; i < MPlayerCount; i++) {
			score_index[i] = -1;

			/*..................................................................
			Search for this player's name in the MPlayerScore array
			..................................................................*/
			for (j = 0; j < MPlayerNumScores; j++) {
				if (!stricmp(MPlayerNames[i],MPlayerScore[j].Name)) {
					score_index[i] = j;
					break;
				}
			}

			/*..................................................................
			If the index is still -1, the name wasn't found; add a new entry.
			..................................................................*/
			if (score_index[i] == -1) {
				if (MPlayerNumScores < MAX_MULTI_NAMES) {
					score_index[i] = MPlayerNumScores;
					MPlayerNumScores++;
				} else {

					/*...............................................................
					For each player in the scores array, count the # of '-1' entries
					from this game backwards; the one with the most is the one that
					hasn't played the longest; replace him with this new guy.
					...............................................................*/
					max_index = 0;
					max_count = 0;
					for (j = 0; j < MPlayerNumScores; j++) {
						count = 0;
						for (k = MPlayerNumScores - 1; k >= 0; k--) {
							if (MPlayerScore[j].Kills[k]==-1) {
								count++;
							} else {
								break;
							}
						}
						if (count > max_count) {
							max_count = count;
							max_index = j;
						}
					}
					score_index[i] = max_index;
				}

				/*...............................................................
				Initialize this score entry
				...............................................................*/
				MPlayerScore[score_index[i]].Wins = 0;
				strcpy (MPlayerScore[score_index[i]].Name,MPlayerNames[i]);
				for (j = 0; j < MAX_MULTI_GAMES; j++)
					MPlayerScore[score_index[i]].Kills[j] = -1;
			}

			/*..................................................................
			Init this player's Kills to 0 (-1 means he didn't play this round;
			0 means he played but got no kills).
			..................................................................*/
			MPlayerScore[score_index[i]].Kills[MPlayerCurGame] = 0;

			/*..................................................................
			Init this player's color to his last-used color index
			..................................................................*/
			MPlayerScore[score_index[i]].Color = MPlayerID_To_ColorIndex(MPlayerID[i]);
		}

#if 0	// (This is the old method of tallying scores:
		/*---------------------------------------------------------------------
		Tally up the scores for this game:
		- For each house:
		  - If this house is human & wasn't defeated, its the winner
		  - If this house was defeated, find out who did it & increment their
		    Kills value.
		---------------------------------------------------------------------*/
		for (house = HOUSE_MULTI1; house < (HOUSE_MULTI1 + MPlayerMax); house++) {
			hptr = HouseClass::As_Pointer(house);
			if (!hptr) continue;

			if (!hptr->IsDefeated) {

				/*...............................................................
				If this is the winning house, find which player it was & increment
				their 'Wins' value
				...............................................................*/
				if (hptr->IsHuman) {
					for (i = 0; i < MPlayerCount; i++) {
						if (house == MPlayerHouses[i]) {
							MPlayerScore[score_index[i]].Wins++;
							MPlayerWinner = score_index[i];
						}
					}
				}
			} else {

				/*..................................................................
				This house was defeated; find which player who defeated him & increment
				his 'Kills' value for this game
				..................................................................*/
				for (i = 0; i < MPlayerCount; i++) {
					if (hptr->WhoLastHurtMe == MPlayerHouses[i]) {
						MPlayerScore[score_index[i]].Kills[MPlayerCurGame]++;
					}
				}
			}
		}

#else	// This is the new method:

		/*---------------------------------------------------------------------
		Tally up the scores for this game:
		- For each player:
		  - If this player is undefeated this round, he's the winner
		  - Each player's Kills value is the sum of the unit's they killed
		---------------------------------------------------------------------*/
		for (i = 0; i < MPlayerCount; i++) {
			hptr = HouseClass::As_Pointer(MPlayerHouses[i]);

			/*..................................................................
			If this house was undefeated, it must have been the winner.  (If
			no human houses are undefeated, the computer won.)
			..................................................................*/
			if (!hptr->IsDefeated) {
				MPlayerScore[score_index[i]].Wins++;
				MPlayerWinner = score_index[i];
			}

			/*..................................................................
			Tally up all kills for this player
			..................................................................*/
			for (house = HOUSE_FIRST; house < HOUSE_COUNT; house++) {

				MPlayerScore[score_index[i]].Kills[MPlayerCurGame] +=
					hptr->UnitsKilled[house];

				MPlayerScore[score_index[i]].Kills[MPlayerCurGame] +=
					hptr->BuildingsKilled[house];
			}
		}
#endif

		/*---------------------------------------------------------------------
		Destroy all the IPX connections, since we have to go through the rest
		of the Main_Loop() before we detect that the game is over, and we'll
		end up waiting for frame sync packets from the other machines.
		---------------------------------------------------------------------*/
		if (GameToPlay==GAME_IPX || GameToPlay == GAME_INTERNET) {
			i = 0;
			while (Ipx.Num_Connections() && (i++ < 1000) ) {
				id = Ipx.Connection_ID(0);
				Ipx.Delete_Connection(id);
			}
			MPlayerCount = 0;
		}
	}

	/*------------------------------------------------------------------------
	Be sure our messages get displayed, even if we're about to exit.
	------------------------------------------------------------------------*/
	Map.Render();
}


/***************************************************************************
 * HouseClass::Blowup_All -- blows up everything                           *
 *                                                                         *
 * INPUT:                                                                  *
 *      none.                                                              *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      none.                                                              *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/16/1995 BRR : Created.                                             *
 *   06/09/1995 JLB : Handles aircraft.                                    *
 *=========================================================================*/
void HouseClass::Blowup_All(void)
{
	Validate();
	int i;
	int damage;
	UnitClass *uptr;
	InfantryClass *iptr;
	BuildingClass *bptr;
	int count;
	WarheadType warhead;

	/*
	**	Find everything owned by this house & blast it with a huge amount of damage
	**	at zero range.  Do units before infantry, so the units' drivers are killed
	**	too.  Using Explosion_Damage is like dropping a big bomb right on the
	**	object; it will also damage anything around it.
	*/
	for (i = 0; i < ::Units.Count(); i++) {
		if (::Units.Ptr(i)->House == this && !::Units.Ptr(i)->IsInLimbo) {
			uptr = ::Units.Ptr(i);

			/*
			**	Some units can't be killed with one shot, so keep damaging them until
			**	they're gone.  The unit will destroy itself, and put an infantry in
			**	its place.  When the unit destroys itself, decrement 'i' since
			**	its pointer will be removed from the active pointer list.
			*/
			count = 0;
			while (::Units.Ptr(i)==uptr && uptr->Strength) {
				damage = 0x7fff;
				Explosion_Damage(uptr->Center_Coord(), damage, NULL, WARHEAD_HE);
				count++;
				if (count > 5) {
					delete uptr;
					break;
				}
			}
			i--;
		}
	}

	/*
	**	Destroy all aircraft owned by this house.
	*/
	for (i = 0; i < ::Aircraft.Count(); i++) {
		if (::Aircraft.Ptr(i)->House == this && !::Aircraft.Ptr(i)->IsInLimbo) {
			AircraftClass * aptr = ::Aircraft.Ptr(i);

			damage = 0x7fff;
			aptr->Take_Damage(damage, 0, WARHEAD_HE, NULL);
			if (!aptr->IsActive) {
				i--;
			}
		}
	}

	/*
	**	Buildings don't delete themselves when they die; they shake the screen
	**	and begin a countdown, so don't decrement 'i' when it's destroyed.
	*/
	for (i = 0; i < Buildings.Count(); i++) {
		if (Buildings.Ptr(i)->House == this && !Buildings.Ptr(i)->IsInLimbo) {
			bptr = Buildings.Ptr(i);

			count = 0;
			bptr->IsSurvivorless = true;
			while (Buildings.Ptr(i)==bptr && bptr->Strength) {
				damage = 0x7fff;
				Explosion_Damage(bptr->Center_Coord(), damage, NULL, WARHEAD_HE);
				count++;
				if (count > 5) {
					delete bptr;
					break;
				}
			}
		}
	}

	/*
	**	Infantry don't delete themselves when they die; they go into a death-
	**	animation sequence, so there's no need to decrement 'i' when they die.
	**	Infantry should die by different types of warheads, so their death
	**	anims aren't all synchronized.
	*/
	for (i = 0; i < Infantry.Count(); i++) {
		if (Infantry.Ptr(i)->House == this && !Infantry.Ptr(i)->IsInLimbo) {
			iptr = Infantry.Ptr(i);

			count = 0;
			while (Infantry.Ptr(i)==iptr && iptr->Strength) {
				damage = 0x7fff;
				warhead = (WarheadType)IRandom (WARHEAD_SA, WARHEAD_FIRE);
				Explosion_Damage(iptr->Center_Coord(), damage, NULL, warhead);
				if (iptr->IsActive) {
					damage = 0x7fff;
					iptr->Take_Damage(damage, 0, warhead);
				}

				count++;
				if (count > 5) {
					delete iptr;
					break;
				}
			}
		}
	}

#ifdef NEVER
	/*
	**	Just delete the teams & triggers for this house.
	*/
	for (i = 0; i < TeamTypes.Count(); i++) {
		if (TeamTypes.Ptr(i)->House == Class->House) {
			delete TeamTypes.Ptr(i);
			i--;
		}
	}
	for (i = 0; i < Triggers.Count(); i++) {
		if (Triggers.Ptr(i)->House == Class->House) {
			delete Triggers.Ptr(i);
			i--;
		}
	}
#endif
}


/***********************************************************************************************
 * HouseClass::Flag_To_Die -- Flags the house to blow up soon.                                 *
 *                                                                                             *
 *    When this routine is called, the house will blow up after a period of time. Typically    *
 *    this is called when the flag is captured or the HQ destroyed.                            *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Was the house flagged to blow up?                                                  *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/20/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Flag_To_Die(void)
{
	Validate();
	if (!IsToWin && !IsToDie && !IsToLose) {
		IsToDie = true;
		if (IsV107) {
			BorrowedTime = TICKS_PER_SECOND * 3;
		} else {
			BorrowedTime = TICKS_PER_SECOND * 1;
		}
	}
	return(IsToDie);
}


/***********************************************************************************************
 * HouseClass::Flag_To_Win -- Flags the house to win soon.                                     *
 *                                                                                             *
 *    When this routine is called, the house will be declared the winner after a period of     *
 *    time.                                                                                    *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Was the house flagged to win?                                                      *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/20/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Flag_To_Win(void)
{
	Validate();
	if (!IsToWin && !IsToDie && !IsToLose) {
		IsToWin = true;
		if (IsV107) {
			BorrowedTime = TICKS_PER_SECOND * 3;
		} else {
			BorrowedTime = TICKS_PER_SECOND * 1;
		}
	}
	return(IsToWin);
}


/***********************************************************************************************
 * HouseClass::Flag_To_Lose -- Flags the house to die soon.                                    *
 *                                                                                             *
 *    When this routine is called, it will spell the doom of this house. In a short while      *
 *    all of the object owned by this house will explode. Typical use of this routine is when  *
 *    the flag has been captured or the command vehicle has been destroyed.                    *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Has the doom been initiated?                                                       *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/12/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Flag_To_Lose(void)
{
	Validate();
	IsToWin = false;
	if (!IsToDie && !IsToLose) {
		IsToLose = true;
		if (IsV107) {
			BorrowedTime = TICKS_PER_SECOND * 3;
		} else {
			BorrowedTime = TICKS_PER_SECOND * 1;
		}
	}
	return(IsToLose);
}


/***********************************************************************************************
 * HouseClass::Init_Data -- Initializes the multiplayer color data.                            *
 *                                                                                             *
 *    This routine is called when initializing the color and remap data for this house. The    *
 *    primary user of this routine is the multiplayer version of the game.                     *
 *                                                                                             *
 * INPUT:   color    -- The color of this house.                                               *
 *                                                                                             *
 *          house    -- The house that this should act like.                                   *
 *                                                                                             *
 *          credits  -- The initial credits to assign to this house.                           *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/29/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Init_Data(PlayerColorType color, HousesType house, int credits)
{
	Validate();
	Credits = InitialCredits = credits;
	ActLike = house;
	RemapColor = color;
	switch (color) {
		case REMAP_YELLOW:
			RemapTable = RemapYellow;
			((unsigned char &)Class->Color) = 157;
			((unsigned char &)Class->BrightColor) = 5;
			break;

		case REMAP_RED:
			RemapTable = RemapRed;
			((unsigned char &)Class->Color) = 123;
			((unsigned char &)Class->BrightColor) = 127;
			break;

		case REMAP_AQUA:
			RemapTable = RemapBlueGreen;
			((unsigned char &)Class->Color) = 135;
			((unsigned char &)Class->BrightColor) = 2;
			break;

		case REMAP_ORANGE:
			RemapTable = RemapOrange;
			((unsigned char &)Class->Color) = 26;
			((unsigned char &)Class->BrightColor) = 24;
			break;

		case REMAP_GREEN:
			RemapTable = RemapGreen;
			((unsigned char &)Class->Color) = 167;
			((unsigned char &)Class->BrightColor) = 159;
			break;

		case REMAP_BLUE:
			RemapTable = RemapBlue;
			((unsigned char &)Class->Color) = 203;
			((unsigned char &)Class->BrightColor) = 201;
			break;
	}
}


/***********************************************************************************************
 * HouseClass::Power_Fraction -- Fetches the current power output rating.                      *
 *                                                                                             *
 *    Use this routine to fetch the current power output as a fixed point fraction. The        *
 *    value 0x0100 is 100% power.                                                              *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Returns with power rating as a fixed pointer number.                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/22/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
int HouseClass::Power_Fraction(void) const
{
	Validate();
	if (Power) {
		if (Drain) {
			return(Cardinal_To_Fixed(Drain, Power));
		} else {
			return(0x0100);
		}
	}
	return(0);
}


/***********************************************************************************************
 * HouseClass::Has_Nuke_Device -- Deteremines if the house has a nuclear device.               *
 *                                                                                             *
 *    This routine checks to see if the house has a nuclear device to launch. A nuclear        *
 *    device is available when the necessary parts have been retrieved in earlier scenarios    *
 *    or if this is the multiplayer version.                                                   *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  Does the house have a nuclear device?                                              *
 *                                                                                             *
 * WARNINGS:   This does not check to see if there is a suitable launch facility (i.e., the    *
 *             Temple of Nod), only that there is a nuclear device potential.                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/24/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool HouseClass::Has_Nuke_Device(void)
{
	Validate();
	if (GameToPlay != GAME_NORMAL || !IsHuman) return(true);
	return((NukePieces & 0x07) == 0x07);
}


/***********************************************************************************************
 * HouseClass::Sell_Wall -- Tries to sell the wall at the specified location.                  *
 *                                                                                             *
 *    This routine will try to sell the wall at the specified location. If there is a wall     *
 *    present and it is owned by this house, then it can be sold.                              *
 *                                                                                             *
 * INPUT:   cell  -- The cell that wall selling is desired.                                    *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/05/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void HouseClass::Sell_Wall(CELL cell)
{
	Validate();
	if ((unsigned)cell > 0) {
		OverlayType overlay = Map[cell].Overlay;

		if (overlay != OVERLAY_NONE && Map[cell].Owner == Class->House) {
			OverlayTypeClass const & optr = OverlayTypeClass::As_Reference(overlay);

			if (optr.IsWall) {
				int cost = 0;
				switch (overlay) {
					case OVERLAY_SANDBAG_WALL:
						cost = BuildingTypeClass::As_Reference(STRUCT_SANDBAG_WALL).Cost_Of();
						break;

					case OVERLAY_CYCLONE_WALL:
						cost = BuildingTypeClass::As_Reference(STRUCT_CYCLONE_WALL).Cost_Of();
						break;

					case OVERLAY_BRICK_WALL:
						cost = BuildingTypeClass::As_Reference(STRUCT_BRICK_WALL).Cost_Of();
						break;

					case OVERLAY_BARBWIRE_WALL:
						cost = BuildingTypeClass::As_Reference(STRUCT_BARBWIRE_WALL).Cost_Of();
						break;

					case OVERLAY_WOOD_WALL:
						cost = BuildingTypeClass::As_Reference(STRUCT_WOOD_WALL).Cost_Of();
						break;

					default:
						cost = 0;
						break;
				}
				Refund_Money(cost/2);
				Map[cell].Overlay = OVERLAY_NONE;
				Map[cell].OverlayData = 0;
				Map[cell].Owner = HOUSE_NONE;
				Map[cell].Wall_Update();
				Map[cell].Recalc_Attributes();
				Map[cell].Redraw_Objects();
				ObjectClass::Detach_This_From_All(::As_Target(cell), true);
			}
		}
	}
}
