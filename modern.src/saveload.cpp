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

/* $Header:   F:\projects\c&c\vcs\code\saveload.cpv   2.18   16 Oct 1995 16:48:44   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : SAVELOAD.CPP                                                 *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : August 23, 1994                                              *
 *                                                                                             *
 *                  Last Update : June 24, 1995 [JLB]                                          *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Code_All_Pointers -- Code all pointers.                                                   *
 *   Decode_All_Pointers -- Decodes all pointers.                                              *
 *   Get_Savefile_Info -- gets description, scenario #, house                                  *
 *   Load_Game -- loads a saved game                                                           *
 *   Load_Misc_Values -- Loads miscellaneous variables.                                        *
 *   Load_Misc_Values -- loads miscellaneous variables                                         *
 *   Read_Object -- reads an object from disk, in a safe way                                   *
 *   Save_Game -- saves a game to disk                                                         *
 *   Save_Misc_Values -- saves miscellaneous variables                                         *
 *   Target_To_TechnoType -- converts TARGET to TechnoTypeClass                                *
 *   TechnoType_To_Target -- converts TechnoTypeClass to TARGET                                *
 *   Write_Object -- reads an object from disk, in a safe way                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "function.h"
#include <dbg.h>
extern const char* TD_Get_Save_Path(void);

/*
********************************** Defines **********************************
*/
#define	SAVEGAME_VERSION		(DESCRIP_MAX +						\
										0x01000003 + ( 					\
										sizeof(AircraftClass) + 		\
										sizeof(AircraftTypeClass) + 	\
										sizeof(AnimClass) + 				\
										sizeof(AnimTypeClass) + 		\
										sizeof(BuildingClass) + 		\
										sizeof(BuildingTypeClass) + 	\
										sizeof(BulletClass) + 			\
										sizeof(BulletTypeClass) + 		\
										sizeof(HouseClass) + 			\
										sizeof(HouseTypeClass) + 		\
										sizeof(InfantryClass) + 		\
										sizeof(InfantryTypeClass) + 	\
										sizeof(OverlayClass) + 			\
										sizeof(OverlayTypeClass) + 	\
										sizeof(SmudgeClass) + 			\
										sizeof(SmudgeTypeClass) + 		\
										sizeof(TeamClass) + 				\
										sizeof(TeamTypeClass) + 		\
										sizeof(TemplateClass) + 		\
										sizeof(TemplateTypeClass) + 	\
										sizeof(TerrainClass) + 			\
										sizeof(TerrainTypeClass) + 	\
										sizeof(UnitClass) + 				\
										sizeof(UnitTypeClass) + 		\
										sizeof(MouseClass) + 			\
										sizeof(CellClass) + 				\
										sizeof(FactoryClass) + 			\
										sizeof(BaseClass) + 				\
										sizeof(LayerClass) +				\
										sizeof(BriefingText) + \
										sizeof(Waypoint)))


/***************************************************************************
 * Save_Game -- saves a game to disk                                       *
 *                                                                         *
 * Saving the Map:                                                         *
 *     DisplayClass::Save() invokes CellClass's Write() for every cell     *
 *     that needs to be saved.  A cell needs to be saved if it contains    *
 *     any special data at all, such as a TIcon, or an Occupier.           *
 *   The cell saves its own CellTrigger pointer, converted to a TARGET.    *
 *                                                                         *
 * Saving game objects:                                                    *
 *   - Any object stored in an ArrayOf class needs to be saved.  The ArrayOf*
 *     Save() routine invokes each object's Write() routine, if that       *
 *     object's IsActive is set.                                           *
 *                                                                         *
 * Saving the layers:                                                      *
 *   The Map's Layers (Ground, Air, etc) of things that are on the map,    *
 *     and the Logic's Layer of things to process both need to be saved.   *
 *     LayerClass::Save() writes the entire layer array to disk            *
 *                                                                         *
 * Saving the houses:                                                      *
 *   Each house needs to be saved, to record its Credits, Power, etc.      *
 *                                                                         *
 * Saving miscellaneous data:                                              *
 *   There are a lot of miscellaneous variables to save, such as the       *
 *     map's dimensions, the player's house, etc.                          *
 *                                                                         *
 * INPUT:                                                                  *
 *      id      numerical ID, for the file extension                       *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      true = OK, false = error                                           *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   12/28/1994 BR : Created.                                              *
 *=========================================================================*/
bool Save_Game(int id,char *descr)
{
	RawFileClass file;
	char name[_MAX_FNAME+_MAX_EXT];
	int i;
	unsigned long version;
	unsigned scenario;
	HousesType house;
	char descr_buf[DESCRIP_MAX];

	scenario = Scenario;								// get current scenario #
	house = PlayerPtr->Class->House;				// get current house

	/*
	**	Generate the filename to save
	*/
	snprintf(name, sizeof(name), "%sSAVEGAME.%03d", TD_Get_Save_Path(), id);

	/*
	**	Code everybody's pointers
	*/
	Code_All_Pointers();

	/*
	**	Open the file
	*/
	fprintf(stderr, "Save_Game: opening %s\n", name);
	if (!file.Open(name, WRITE)) {
		fprintf(stderr, "Save_Game: FAILED to open file\n");
		Decode_All_Pointers();
		return(false);
	}
	fprintf(stderr, "Save_Game: file opened OK\n");

	/*
	**	Save the description, scenario #, and house
	**	(scenario # & house are saved separately from the actual Scenario &
	**	PlayerPtr globals for convenience; we can quickly find out which
	**	house & scenario this save-game file is for by reading these values.
	**	Also, PlayerPtr is stored in a coded form in Save_Misc_Values(),
	**	which may or may not be a HousesType number; so, saving 'house'
	**	here ensures we can always pull out the house for this file.)
	*/
	sprintf(descr_buf, "%s\r\n",descr);			// put CR-LF after text
	descr_buf[strlen(descr_buf) + 1] = 26;		// put CTRL-Z after NULL

	if (file.Write(descr_buf, DESCRIP_MAX) != DESCRIP_MAX) {
		file.Close();
		return(false);
	}

	if (file.Write(&scenario, sizeof(scenario)) != sizeof(scenario)) {
		file.Close();
		return(false);
	}

	if (file.Write(&house, sizeof(house)) != sizeof(house)) {
		file.Close();
		return(false);
	}

	/*
	**	Save the save-game version, for loading verification
	*/
	version = SAVEGAME_VERSION;

	if (file.Write(&version, sizeof(version)) != sizeof(version)) {
		file.Close();
		return(false);
	}

	Call_Back();
	/*
	**	Save the map.  The map must be saved first, since it saves the Theater.
	*/
	Map.Save(file);

	Call_Back();
	/*
	**	Save all game objects.  This code saves every object that's stored in a
	**	TFixedIHeap class.
	*/
	fprintf(stderr, "Save_Game: saving objects...\n");
	fprintf(stderr, "  Houses: ActiveCount=%d, file pos=%ld\n", Houses.Count(), (long)file.Seek(0, SEEK_CUR));
	#define SAVE_CHECK(name, expr) \
		if (!(expr)) { fprintf(stderr, "Save_Game: FAILED at %s (file pos=%ld)\n", name, (long)file.Seek(0, SEEK_CUR)); file.Close(); Decode_All_Pointers(); return false; } \
		else fprintf(stderr, "Save_Game: %s OK (file pos=%ld)\n", name, (long)file.Seek(0, SEEK_CUR));
	/* Inline Houses.Save with tracing */
	{
		int hcount = Houses.Count();
		fprintf(stderr, "  Houses: writing count=%d\n", hcount);
		int wrote = file.Write(&hcount, sizeof(hcount));
		fprintf(stderr, "  Houses: wrote count %d bytes (expected %zu)\n", wrote, sizeof(hcount));
		if (wrote != sizeof(hcount)) {
			fprintf(stderr, "Save_Game: FAILED writing Houses count\n");
			file.Close(); Decode_All_Pointers(); return false;
		}
		for (int hi = 0; hi < hcount; hi++) {
			int idx = Houses.ID(Houses.Ptr(hi));
			wrote = file.Write(&idx, sizeof(idx));
			if (wrote != sizeof(idx)) {
				fprintf(stderr, "  Houses: FAILED writing idx for house %d\n", hi);
				file.Close(); Decode_All_Pointers(); return false;
			}
			if (!Houses.Ptr(hi)->Save(file)) {
				fprintf(stderr, "  Houses: FAILED saving house %d (idx=%d)\n", hi, idx);
				file.Close(); Decode_All_Pointers(); return false;
			}
			fprintf(stderr, "  Houses: saved house %d OK\n", hi);
		}
		fprintf(stderr, "Save_Game: Houses OK\n");
	}
	/* Inline TeamTypes.Save with tracing */
	{
		int cnt = TeamTypes.Count();
		fprintf(stderr, "  TeamTypes: count=%d\n", cnt);
		if (file.Write(&cnt, sizeof(cnt)) != sizeof(cnt)) {
			fprintf(stderr, "Save_Game: FAILED writing TeamTypes count\n");
			file.Close(); Decode_All_Pointers(); return false;
		}
		for (int ti = 0; ti < cnt; ti++) {
			int idx = TeamTypes.ID(TeamTypes.Ptr(ti));
			if (file.Write(&idx, sizeof(idx)) != sizeof(idx)) {
				fprintf(stderr, "  TeamTypes: FAILED writing idx %d\n", ti);
				file.Close(); Decode_All_Pointers(); return false;
			}
			if (!TeamTypes.Ptr(ti)->Save(file)) {
				fprintf(stderr, "  TeamTypes: FAILED saving item %d (idx=%d, sizeof=%zu)\n", ti, idx, sizeof(TeamTypeClass));
				file.Close(); Decode_All_Pointers(); return false;
			}
		}
		fprintf(stderr, "Save_Game: TeamTypes OK\n");
	}
	/*
	** Inline pool saves — TFixedIHeapClass::Save virtual call is broken
	** on LP64 (template instantiation issue). Use this macro instead.
	*/
	#define POOL_SAVE(label, pool) do { \
		int _cnt = pool.Count(); \
		if (file.Write(&_cnt, sizeof(_cnt)) != sizeof(_cnt)) { \
			fprintf(stderr, "Save_Game: FAILED writing %s count\n", label); \
			file.Close(); Decode_All_Pointers(); return false; \
		} \
		for (int _i = 0; _i < _cnt; _i++) { \
			int _idx = pool.ID(pool.Ptr(_i)); \
			if (file.Write(&_idx, sizeof(_idx)) != sizeof(_idx)) { \
				fprintf(stderr, "Save_Game: FAILED writing %s idx %d\n", label, _i); \
				file.Close(); Decode_All_Pointers(); return false; \
			} \
			if (!pool.Ptr(_i)->Save(file)) { \
				fprintf(stderr, "Save_Game: FAILED saving %s item %d\n", label, _i); \
				file.Close(); Decode_All_Pointers(); return false; \
			} \
		} \
		fprintf(stderr, "Save_Game: %s OK (%d items)\n", label, _cnt); \
	} while(0)

	POOL_SAVE("Teams", Teams);
	POOL_SAVE("Triggers", Triggers);
	POOL_SAVE("Aircraft", Aircraft);
	POOL_SAVE("Anims", Anims);
	POOL_SAVE("Buildings", Buildings);
	POOL_SAVE("Bullets", Bullets);
	POOL_SAVE("Infantry", Infantry);
	POOL_SAVE("Overlays", Overlays);
	POOL_SAVE("Smudges", Smudges);
	POOL_SAVE("Templates", Templates);
	POOL_SAVE("Terrains", Terrains);
	POOL_SAVE("Units", Units);
	POOL_SAVE("Factories", Factories);

	Call_Back();
	/*
	**	Save the Logic & Map layers
	*/
	SAVE_CHECK("Logic", Logic.Save(file));

	for (i = 0; i < LAYER_COUNT; i++) {
		if (!Map.Layer[i].Save(file)) {
			file.Close();
			Decode_All_Pointers();
			return(false);
		}
	}

	/*
	**	Save the Score
	*/
	if (!Score.Save(file)) {
		file.Close();
		Decode_All_Pointers();
		return(false);
	}

	/*
	**	Save the AI Base
	*/
	if (!Base.Save(file)) {
		file.Close();
		Decode_All_Pointers();
		return(false);
	}

	/*
	**	Save miscellaneous variables.
	*/
	if (!Save_Misc_Values(file)) {
		file.Close();
		Decode_All_Pointers();
		return(false);
	}

	Call_Back();
	/*
	**	Close the file; we're done
	*/
	file.Close();
	Decode_All_Pointers();

	return(true);
}


/***************************************************************************
 * Load_Game -- loads a saved game                                         *
 *                                                                         *
 * This routine loads the data in the same way it was saved out.           *
 *                                                                         *
 * Loading the Map:                                                        *
 *   - DisplayClass::Load() invokes CellClass's Load() for every cell      *
 *     that was saved.                                                     *
 * - The cell loads its own CellTrigger pointer.                           *
 *                                                                         *
 * Loading game objects:                                                   *
 * - IHeap's Load() routine loads the # of objects stored, and loads       *
 *   each object.                                                          *
 * - Triggers: Add themselves to the HouseTriggers if they're associated   *
 *   with a house                                                          *
 *                                                                         *
 * Loading the layers:                                                     *
 *     LayerClass::Load() reads the entire layer array to disk             *
 *                                                                         *
 * Loading the houses:                                                     *
 *   Each house is loaded in its entirety.                                 *
 *                                                                         *
 * Loading miscellaneous data:                                             *
 *   There are a lot of miscellaneous variables to load, such as the       *
 *     map's dimensions, the player's house, etc.                          *
 *                                                                         *
 * INPUT:                                                                  *
 *      id         numerical ID, for the file extension                    *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      true = OK, false = error                                           *
 *                                                                         *
 * WARNINGS:                                                               *
 *      If this routine returns false, the entire game will be in an       *
 *      unknown state, so the scenario will have to be re-initialized.     *
 *                                                                         *
 * HISTORY:                                                                *
 *   12/28/1994 BR : Created.                                              *
 *=========================================================================*/
bool Load_Game(int id)
{
	RawFileClass file;
	char name[_MAX_FNAME+_MAX_EXT];
	int i;
	unsigned long version;
	unsigned scenario;
	HousesType house;
	char descr_buf[DESCRIP_MAX];

	/*
	**	Generate the filename to load
	*/
	snprintf(name, sizeof(name), "%sSAVEGAME.%03d", TD_Get_Save_Path(), id);

	/*
	**	Open the file
	*/
	fprintf(stderr, "Load_Game: opening '%s'\n", name);
	if (!file.Open(name, READ)) {
		fprintf(stderr, "Load_Game: FAILED to open file\n");
		return(false);
	}
	fprintf(stderr, "Load_Game: file opened OK\n");

	/*
	**	Read & discard the save-game's header info
	*/
	if (file.Read(descr_buf, DESCRIP_MAX) != DESCRIP_MAX) {
		fprintf(stderr, "Load_Game: FAILED reading description\n");
		file.Close();
		return(false);
	}
	fprintf(stderr, "Load_Game: descr='%.40s'\n", descr_buf);

	if (file.Read(&scenario, sizeof(scenario)) != sizeof(scenario)) {
		fprintf(stderr, "Load_Game: FAILED reading scenario\n");
		file.Close();
		return(false);
	}
	fprintf(stderr, "Load_Game: scenario=%u\n", scenario);

	if (file.Read(&house, sizeof(house)) != sizeof(house)) {
		fprintf(stderr, "Load_Game: FAILED reading house\n");
		file.Close();
		return(false);
	}
	fprintf(stderr, "Load_Game: house=%d\n", (int)house);

	Call_Back();
	Clear_Scenario();

	/*
	**	Read in & verify the save-game ID code
	*/
	if (file.Read(&version,sizeof(version)) != sizeof(version)) {
		fprintf(stderr, "Load_Game: FAILED reading version\n");
		file.Close();
		return(false);
	}

	fprintf(stderr, "Load_Game: file_version=0x%lx, expected=0x%lx\n",
		version, (unsigned long)SAVEGAME_VERSION);

	if (version != SAVEGAME_VERSION) {
		fprintf(stderr, "Load_Game: VERSION MISMATCH!\n");
		file.Close();
		return(false);
	}
	fprintf(stderr, "Load_Game: version OK\n");

	Call_Back();
	/*
	**	Set the required CD to be in the drive according to the scenario
	**	loaded.
	*/
	if (RequiredCD != -2) {
		if (scenario >= 20 && scenario <60 && GameToPlay == GAME_NORMAL) {
			RequiredCD = 2;
		} else {
			if (scenario >= 60){
				/*
				** This is a gateway bonus scenario
				*/
				RequiredCD = -1;
			}else{
				if (house == HOUSE_GOOD) {
					RequiredCD = 0;
				} else {
					RequiredCD = 1;
				}
			}
		}
	}
	if(!Force_CD_Available(RequiredCD)) {
		Prog_End();
		exit(EXIT_FAILURE);
	}

	Call_Back();

	/*
	**	Load the map.  The map comes first, since it loads the Theater & init's
	**	mixfiles.  The map calls all the type-class's Init routines, telling them
	**	what the Theater is; this must be done before any objects are created, so
	**	they'll be properly created.
	*/
	fprintf(stderr, "Load_Game: loading map (file pos=%ld)...\n", (long)file.Seek(0, SEEK_CUR));
	Map.Load(file);
	fprintf(stderr, "Load_Game: map loaded (file pos=%ld)\n", (long)file.Seek(0, SEEK_CUR));

	Call_Back();
	/*
	** Inline pool loads — TFixedIHeapClass::Load virtual call is broken
	** on LP64 (same template instantiation issue as Save had).
	** This macro replicates the heap Load logic with correct typing.
	*/
	#define POOL_LOAD(label, pool, type) do { \
		int _cnt; \
		fprintf(stderr, "Load_Game: loading %s (file pos=%ld)...\n", label, (long)file.Seek(0, SEEK_CUR)); \
		if (file.Read(&_cnt, sizeof(_cnt)) != sizeof(_cnt)) { \
			fprintf(stderr, "Load_Game: FAILED reading %s count\n", label); \
			file.Close(); return false; \
		} \
		fprintf(stderr, "  %s: count=%d\n", label, _cnt); \
		for (int _i = 0; _i < _cnt; _i++) { \
			int _idx; \
			if (file.Read(&_idx, sizeof(_idx)) != sizeof(_idx)) { \
				fprintf(stderr, "  %s: FAILED reading idx for item %d\n", label, _i); \
				file.Close(); return false; \
			} \
			type *_ptr = (type *)((char *)pool.Buffer + (_idx * pool.Size)); \
			pool.FreeFlag[_idx] = true; \
			pool.ActiveCount++; \
			pool.ActivePointers.Add(_ptr); \
			if (!_ptr->Load(file)) { \
				fprintf(stderr, "  %s: FAILED loading item %d (idx=%d)\n", label, _i, _idx); \
				file.Close(); return false; \
			} \
		} \
		fprintf(stderr, "Load_Game: %s OK (%d items)\n", label, _cnt); \
	} while(0)

	POOL_LOAD("Houses", Houses, HouseClass);
	POOL_LOAD("TeamTypes", TeamTypes, TeamTypeClass);
	POOL_LOAD("Teams", Teams, TeamClass);
	POOL_LOAD("Triggers", Triggers, TriggerClass);
	POOL_LOAD("Aircraft", Aircraft, AircraftClass);
	POOL_LOAD("Anims", Anims, AnimClass);
	POOL_LOAD("Buildings", Buildings, BuildingClass);
	POOL_LOAD("Bullets", Bullets, BulletClass);
	POOL_LOAD("Infantry", Infantry, InfantryClass);
	POOL_LOAD("Overlays", Overlays, OverlayClass);
	POOL_LOAD("Smudges", Smudges, SmudgeClass);
	POOL_LOAD("Templates", Templates, TemplateClass);
	POOL_LOAD("Terrains", Terrains, TerrainClass);
	POOL_LOAD("Units", Units, UnitClass);
	POOL_LOAD("Factories", Factories, FactoryClass);

	Call_Back();
	/*
	**	Load the Logic & Map Layers
	*/
	fprintf(stderr, "Load_Game: loading Logic (file pos=%ld)...\n", (long)file.Seek(0, SEEK_CUR));
	if (!Logic.Load(file)) {
		fprintf(stderr, "Load_Game: FAILED loading Logic\n");
		file.Close();
		return(false);
	}
	fprintf(stderr, "Load_Game: Logic OK\n");
	for (i = 0; i < LAYER_COUNT; i++) {
		fprintf(stderr, "Load_Game: loading Layer[%d] (file pos=%ld)...\n", i, (long)file.Seek(0, SEEK_CUR));
		if (!Map.Layer[i].Load(file)) {
			fprintf(stderr, "Load_Game: FAILED loading Layer[%d]\n", i);
			file.Close();
			return(false);
		}
	}
	fprintf(stderr, "Load_Game: Layers OK\n");

	Call_Back();
	/*
	**	Load the Score
	*/
	fprintf(stderr, "Load_Game: loading Score (file pos=%ld)...\n", (long)file.Seek(0, SEEK_CUR));
	if (!Score.Load(file)) {
		fprintf(stderr, "Load_Game: FAILED loading Score\n");
		file.Close();
		return(false);
	}
	fprintf(stderr, "Load_Game: Score OK\n");

	/*
	**	Load the AI Base
	*/
	fprintf(stderr, "Load_Game: loading Base (file pos=%ld)...\n", (long)file.Seek(0, SEEK_CUR));
	if (!Base.Load(file)) {
		fprintf(stderr, "Load_Game: FAILED loading Base\n");
		file.Close();
		return(false);
	}
	fprintf(stderr, "Load_Game: Base OK\n");

	/*
	**	Load miscellaneous variables, including the map size & the Theater
	*/
	fprintf(stderr, "Load_Game: loading Misc (file pos=%ld)...\n", (long)file.Seek(0, SEEK_CUR));
	if (!Load_Misc_Values(file)) {
		fprintf(stderr, "Load_Game: FAILED loading Misc\n");
		file.Close();
		return(false);
	}
	fprintf(stderr, "Load_Game: Misc OK\n");

	file.Close();
	fprintf(stderr, "Load_Game: file closed, calling Decode_All_Pointers...\n");
	Decode_All_Pointers();

	/*
	** Recalculate UI positions for the current screen width.
	** Map.Load() restores member variables from the save file which may
	** have been saved at a different resolution.
	*/
	Map.Recalc_Positions();

	fprintf(stderr, "Load_Game: Decode_All_Pointers OK, calling Init_IO...\n");
	Map.Init_IO();
	fprintf(stderr, "Load_Game: Init_IO OK, calling Flag_To_Redraw...\n");
	Map.Flag_To_Redraw(true);
	fprintf(stderr, "Load_Game: Flag_To_Redraw OK\n");

	ScenarioInit = 0;

	/*
	**	Post-load fixup: verify all terrain objects are properly linked.
	**	The cell occupier chain is rebuilt from coded TARGET pointers during
	**	Decode_All_Pointers. If any link decodes to NULL the chain breaks
	**	and Cell_Terrain() can no longer find the object.  Walk each
	**	terrain's actual occupy-list cells to verify linkage, and re-place
	**	any that are missing.  Also ensure every terrain is in the Logic
	**	layer for AI processing.
	*/
	{
		int logic_fixed = 0;
		int cell_fixed = 0;
		int blossom_count = 0;
		for (int ti = 0; ti < Terrains.Count(); ti++) {
			TerrainClass *tp = Terrains.Ptr(ti);
			if (!tp->IsActive || tp->IsInLimbo) continue;

			if (tp->Class->IsTiberiumSpawn) blossom_count++;

			/*
			** Ensure terrain is in the Logic layer for AI processing.
			*/
			bool in_logic = false;
			for (int li = 0; li < Logic.Count(); li++) {
				if (Logic[li] == tp) { in_logic = true; break; }
			}
			if (!in_logic) {
				Logic.Submit(tp);
				logic_fixed++;
				DBG("Load_Game: terrain %s at cell %d re-added to Logic",
					tp->Class->IniName, (int)Coord_Cell(tp->Coord));
			}

			/*
			** Walk the occupy list to find the actual cells this terrain
			** should be in, and verify it appears in each cell's occupier
			** chain.  If missing from any, pick up and re-place.
			*/
			CELL base = Coord_Cell(tp->Coord);
			short const *olist = tp->Occupy_List();
			bool in_cell = true;
			while (*olist != REFRESH_EOL) {
				CELL ocell = base + *olist++;
				if ((unsigned)ocell >= MAP_CELL_TOTAL) continue;
				bool found = false;
				ObjectClass *occ = Map[ocell].Cell_Occupier();
				while (occ) {
					if (occ == tp) { found = true; break; }
					occ = occ->Next;
				}
				if (!found) { in_cell = false; break; }
			}
			if (!in_cell) {
				if (tp->IsDown) {
					tp->Mark(MARK_UP);
				}
				tp->Mark(MARK_DOWN);
				cell_fixed++;
				DBG("Load_Game: terrain %s at cell %d re-placed on map",
					tp->Class->IniName, (int)base);
			}
		}
		DBG("Load_Game: terrain=%d blossom=%d fixup Logic:%d Cell:%d",
			Terrains.Count(), blossom_count, logic_fixed, cell_fixed);

		/*
		** Diagnostic: for each blossom tree, verify Cell_Terrain() finds it
		** and report its animation state.
		*/
		for (int ti = 0; ti < Terrains.Count(); ti++) {
			TerrainClass *tp = Terrains.Ptr(ti);
			if (!tp->IsActive || tp->IsInLimbo) continue;
			if (!tp->Class->IsTiberiumSpawn) continue;

			CELL base = Coord_Cell(tp->Coord);
			short const *olist = tp->Occupy_List();
			CELL ocell = base + olist[0];
			TerrainClass *found = (ocell < MAP_CELL_TOTAL) ? Map[ocell].Cell_Terrain() : NULL;

			DBG("Load_Game: blossom %s base=%d occupy=%d found=%s "
				"down=%d stage=%d rate=%d",
				tp->Class->IniName, (int)base, (int)ocell,
				found == tp ? "YES" : (found ? "WRONG" : "NULL"),
				(int)tp->IsDown,
				(int)tp->Fetch_Stage(), (int)tp->Fetch_Rate());
		}
		DBG("Load_Game: IsTGrowth=%d IsTSpread=%d",
			(int)Special.IsTGrowth, (int)Special.IsTSpread);
	}

#ifdef DEMO
	if (Scenario != 10 && Scenario != 1 && Scenario != 6) {
		Clear_Scenario();
		return(false);
	}
#endif

	Call_Back();
	return(true);
}


/***************************************************************************
 * Save_Misc_Values -- saves miscellaneous variables                       *
 *                                                                         *
 * INPUT:                                                                  *
 *      file      file to use for writing                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      true = success, false = failure                                    *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   12/29/1994 BR : Created.                                              *
 *=========================================================================*/
bool Save_Misc_Values(FileClass &file)
{
	int i;
	int count;								// # ptrs in 'CurrentObject'
	ObjectClass * ptr;					// for saving 'CurrentObject' ptrs

	/*
	**	Player's House.
	*/
	if (file.Write(&PlayerPtr, sizeof(PlayerPtr)) != sizeof(PlayerPtr)) {
		return(false);
	}

	/*
	**	Save this scenario number.
	*/
	if (file.Write(&Scenario, sizeof(Scenario)) != sizeof(Scenario)) {
		return(false);
	}

	/*
	**	Save frame #.
	*/
	if (file.Write(&Frame, sizeof(Frame)) != sizeof(Frame)) {
		return(false);
	}

	/*
	**	Save VQ Movie names.
	*/
	if (file.Write(WinMovie, sizeof(WinMovie)) != sizeof(WinMovie)) {
		return(false);
	}

	if (file.Write(LoseMovie, sizeof(LoseMovie)) != sizeof(LoseMovie)) {
		return(false);
	}

	/*
	**	Save currently-selected objects list.
	**	Save the # of ptrs in the list.
	*/
	count = CurrentObject.Count();
	if (file.Write(&count, sizeof(count)) != sizeof(count)) {
		return(false);
	}

	/*
	**	Save the pointers.
	*/
	for (i = 0; i < count; i++) {
		ptr = CurrentObject[i];
		if (file.Write(&ptr, sizeof(ptr)) != sizeof(ptr)) {
			return(false);
		}
	}

	/*
	**	Save the list of waypoints.
	*/
	if (file.Write(Waypoint, sizeof(Waypoint)) != sizeof(Waypoint)) {
		return(false);
	}

	file.Write(&ScenDir, sizeof(ScenDir));
	file.Write(&ScenVar, sizeof(ScenVar));
	file.Write(&CarryOverMoney, sizeof(CarryOverMoney));
	file.Write(&CarryOverPercent, sizeof(CarryOverPercent));
	file.Write(&BuildLevel, sizeof(BuildLevel));
	file.Write(BriefMovie, sizeof(BriefMovie));
	file.Write(Views, sizeof(Views));
	file.Write(&EndCountDown, sizeof(EndCountDown));
	file.Write(BriefingText, sizeof(BriefingText));

	// This is new...
	file.Write(ActionMovie, sizeof(ActionMovie));

	file.Write(&Special, sizeof(Special));

	return(true);
}


/***********************************************************************************************
 * Load_Misc_Values -- Loads miscellaneous variables.                                          *
 *                                                                                             *
 * INPUT:   file  -- The file to load the misc values from.                                    *
 *                                                                                             *
 * OUTPUT:  Was the misc load process successful?                                              *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/24/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
bool Load_Misc_Values(FileClass &file)
{
	int i;
	int count;								// # ptrs in 'CurrentObject'
	ObjectClass * ptr;					// for loading 'CurrentObject' ptrs

	/*
	**	Player's House.
	*/
	if (file.Read(&PlayerPtr, sizeof(PlayerPtr)) != sizeof(PlayerPtr)) {
		return(false);
	}

	/*
	**	Read this scenario number.
	*/
	if (file.Read(&Scenario,sizeof(Scenario)) != sizeof(Scenario)) {
		return(false);
	}

	/*
	**	Load frame #.
	*/
	if (file.Read(&Frame, sizeof(Frame)) != sizeof(Frame)) {
		return(false);
	}

	/*
	**	Load VQ Movie names.
	*/
	if (file.Read(WinMovie, sizeof(WinMovie)) != sizeof(WinMovie)) {
		return(false);
	}

	if (file.Read(LoseMovie, sizeof(LoseMovie)) != sizeof(LoseMovie)) {
		return(false);
	}

	/*
	**	Load currently-selected objects list.
	**	Load the # of ptrs in the list.
	*/
	if (file.Read(&count, sizeof(count)) != sizeof(count)) {
		return(false);
	}

	/*
	**	Load the pointers.
	*/
	for (i = 0; i < count; i++) {
		if (file.Read(&ptr, sizeof(ptr)) != sizeof(ptr)) {
			return(false);
		}
		CurrentObject.Add(ptr);	// add to the list
	}

	/*
	**	Save the list of waypoints.
	*/
	if (file.Read(Waypoint, sizeof(Waypoint)) != sizeof(Waypoint)) {
		return(false);
	}

	file.Read(&ScenDir, sizeof(ScenDir));
	file.Read(&ScenVar, sizeof(ScenVar));
	file.Read(&CarryOverMoney, sizeof(CarryOverMoney));
	file.Read(&CarryOverPercent, sizeof(CarryOverPercent));
	file.Read(&BuildLevel, sizeof(BuildLevel));
	file.Read(BriefMovie, sizeof(BriefMovie));
	file.Read(Views, sizeof(Views));
	file.Read(&EndCountDown, sizeof(EndCountDown));
	file.Read(BriefingText, sizeof(BriefingText));

	if (file.Seek(0, SEEK_CUR) < file.Size()) {
		file.Read(ActionMovie, sizeof(ActionMovie));
	}

	/*
	**	Special flags (IsTGrowth, IsTSpread, etc.) were not saved in
	**	the original game.  Read them if present, otherwise default.
	*/
	if (file.Seek(0, SEEK_CUR) < file.Size()) {
		file.Read(&Special, sizeof(Special));
	} else {
		Special.Init();
	}

	return(true);
}


/***********************************************************************************************
 * Code_All_Pointers -- Code all pointers.                                                     *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/24/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
void Code_All_Pointers(void)
{
	int i;

	/*
	** Inline pool Code_Pointers — TFixedIHeapClass template virtual is broken on LP64.
	** Each object's Code_Pointers() is a regular virtual call that works fine.
	*/
	#define POOL_CODE(pool, type) do { \
		for (int _i = 0; _i < pool.Count(); _i++) { \
			((type *)pool.ActivePointers[_i])->Code_Pointers(); \
		} \
	} while(0)

	/*
	**	The Map.
	*/
	Map.Code_Pointers();

	/*
	**	The ArrayOf's.
	*/
	POOL_CODE(TeamTypes, TeamTypeClass);
	POOL_CODE(Teams, TeamClass);
	POOL_CODE(Triggers, TriggerClass);
	POOL_CODE(Aircraft, AircraftClass);
	POOL_CODE(Anims, AnimClass);
	POOL_CODE(Buildings, BuildingClass);
	POOL_CODE(Bullets, BulletClass);
	POOL_CODE(Infantry, InfantryClass);
	POOL_CODE(Overlays, OverlayClass);
	POOL_CODE(Smudges, SmudgeClass);
	POOL_CODE(Templates, TemplateClass);
	POOL_CODE(Terrains, TerrainClass);
	POOL_CODE(Units, UnitClass);
	POOL_CODE(Factories, FactoryClass);

	/*
	**	The Layers.
	*/
	Logic.Code_Pointers();
	for (i = 0; i < LAYER_COUNT; i++) {
		Map.Layer[i].Code_Pointers();
	}

	/*
	**	The Score.
	*/
	Score.Code_Pointers();

	/*
	**	The Base.
	*/
	Base.Code_Pointers();

	/*
	**	PlayerPtr.
	*/
	PlayerPtr = (HouseClass *)(PlayerPtr->Class->House);

	/*
	**	Currently-selected objects.
	*/
	for (i = 0; i < CurrentObject.Count(); i++) {
		CurrentObject[i] = (ObjectClass *)(intptr_t)CurrentObject[i]->As_Target();
	}

	/*
	** Houses must be coded last, because the Class->House member of the HouseClass
	** is used to code HouseClass pointers for all other objects, and if Class is
	** coded, it will point to a meaningless value.
	*/
	POOL_CODE(Houses, HouseClass);
}


/***********************************************************************************************
 * Decode_All_Pointers -- Decodes all pointers.                                                *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/24/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
void Decode_All_Pointers(void)
{
	int i;

	/*
	** Inline pool Decode_Pointers — TFixedIHeapClass template virtual is broken on LP64.
	*/
	#define POOL_DECODE(label, pool, type) do { \
		fprintf(stderr, "  Decode: %s (%d items)...\n", label, pool.Count()); \
		for (int _i = 0; _i < pool.Count(); _i++) { \
			((type *)pool.ActivePointers[_i])->Decode_Pointers(); \
		} \
		fprintf(stderr, "  Decode: %s OK\n", label); \
	} while(0)

	Map.Decode_Pointers();
	fprintf(stderr, "  Decode: Map OK\n");

	POOL_DECODE("Houses", Houses, HouseClass);
	POOL_DECODE("TeamTypes", TeamTypes, TeamTypeClass);
	POOL_DECODE("Teams", Teams, TeamClass);
	POOL_DECODE("Triggers", Triggers, TriggerClass);
	POOL_DECODE("Aircraft", Aircraft, AircraftClass);
	POOL_DECODE("Anims", Anims, AnimClass);
	POOL_DECODE("Buildings", Buildings, BuildingClass);
	POOL_DECODE("Bullets", Bullets, BulletClass);
	POOL_DECODE("Infantry", Infantry, InfantryClass);
	POOL_DECODE("Overlays", Overlays, OverlayClass);
	POOL_DECODE("Smudges", Smudges, SmudgeClass);
	POOL_DECODE("Templates", Templates, TemplateClass);
	POOL_DECODE("Terrains", Terrains, TerrainClass);
	POOL_DECODE("Units", Units, UnitClass);
	POOL_DECODE("Factories", Factories, FactoryClass);

	Logic.Decode_Pointers();
	fprintf(stderr, "  Decode: Logic OK\n");
	for (i = 0; i < LAYER_COUNT; i++) {
		Map.Layer[i].Decode_Pointers();
	}
	fprintf(stderr, "  Decode: Layers OK\n");

	Score.Decode_Pointers();
	fprintf(stderr, "  Decode: Score OK\n");
	Base.Decode_Pointers();
	fprintf(stderr, "  Decode: Base OK\n");

	/*
	**	PlayerPtr.
	*/
	fprintf(stderr, "  Decode: PlayerPtr raw=%p (as HousesType=%d)...\n",
		(void*)PlayerPtr, (int)(intptr_t)PlayerPtr);
	PlayerPtr = HouseClass::As_Pointer((HousesType)(intptr_t)PlayerPtr);
	fprintf(stderr, "  Decode: PlayerPtr decoded=%p\n", (void*)PlayerPtr);
	if (!PlayerPtr) {
		fprintf(stderr, "  Decode: ERROR PlayerPtr is NULL!\n");
	} else {
		fprintf(stderr, "  Decode: PlayerPtr->Class=%p\n", (void*)PlayerPtr->Class);
		Whom = PlayerPtr->Class->House;
		switch (PlayerPtr->Class->House) {
			case HOUSE_GOOD:
				ScenPlayer = SCEN_PLAYER_GDI;
				break;

			case HOUSE_BAD:
				ScenPlayer = SCEN_PLAYER_NOD;
				break;

			case HOUSE_JP:
				ScenPlayer = SCEN_PLAYER_JP;
				break;
		}
	}
	Check_Ptr(PlayerPtr,__FILE__,__LINE__);
	fprintf(stderr, "  Decode: PlayerPtr OK\n");

	Set_Scenario_Name(ScenarioName, Scenario, ScenPlayer, ScenDir, ScenVar);
	fprintf(stderr, "  Decode: ScenarioName OK\n");

	/*
	**	Currently-selected objects.
	*/
	fprintf(stderr, "  Decode: CurrentObject count=%d\n", CurrentObject.Count());
	for (i = 0; i < CurrentObject.Count(); i++) {
		fprintf(stderr, "  Decode: CurrentObject[%d] raw=%p\n", i, (void*)CurrentObject[i]);
		CurrentObject[i] = As_Object((TARGET)(intptr_t)CurrentObject[i]);
		fprintf(stderr, "  Decode: CurrentObject[%d] decoded=%p\n", i, (void*)CurrentObject[i]);
		Check_Ptr(CurrentObject[i],__FILE__,__LINE__);
	}
	fprintf(stderr, "  Decode: CurrentObject OK\n");

	/*
	**	Last-Minute Fixups; to resolve these pointers properly requires all other
	**	pointers to be loaded & decoded.
	*/
	fprintf(stderr, "  Decode: PendingObjectPtr=%p\n", (void*)Map.PendingObjectPtr);
	if (Map.PendingObjectPtr) {
		Map.PendingObject = &Map.PendingObjectPtr->Class_Of();
		Check_Ptr((void *)Map.PendingObject, __FILE__, __LINE__);
		Map.Set_Cursor_Shape(Map.PendingObject->Occupy_List(true));
	} else {
		Map.PendingObject = 0;
		Map.Set_Cursor_Shape(0);
	}
	fprintf(stderr, "  Decode: PendingObject OK\n");
}


/***********************************************************************************************
 * Read_Object -- reads an object from disk                                                    *
 *                                                                                             *
 * This routine reads in an object and fills in the virtual function table pointer.            *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      ptr            pointer to object to read                                               *
 *      base_size      size of object's absolute base class                                    *
 *      class_size      size of the class itself                                               *
 *      file            file to use for I/O                                                    *
 *      vtable         virtual function table pointer value, NULL if none                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      true = OK, false = error                                                               *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      This routine ASSUMES the program modules are compiled with:                            *
 *      -Vb-      Always make the virtual function table ptr 2 bytes long                      *
 *      -Vt      Put the virtual function table after the 1st class's data                     *
 *                                                                                             *
 *      ALSO, the class used to compute 'base_size' must come first in a multiple-inheritence  *
 *      hierarchy.  AND, if your class multiply-inherits from other classes, only ONE of those *
 *      classes can contain virtual functions!  If you include virtual functions in the other  *
 *      classes, the compiler will generate multiple virtual function tables, and this load/save *
 *      technique will fail.                                                                   *
 *                                                                                             *
 *      Each class hierarchy is stored in memory as a chain: first the data for the base-est   *
 *      class, then the virtual function table pointer for this hierarchy, then the data for   *
 *      all derived classes.  If any of these derived classes multiply-inherit, the base class *
 *      for the multiple inheritance is stored as a separate chain following this chain.  The  *
 *      new chain will contain its own virtual function table pointer, if the multiply-        *
 *      inherited hierarchy contains any virtual functions.  Thus, the declaration             *
 *         class A                                                                             *
 *         class B: public A                                                                   *
 *         class C: public B, X                                                                *
 *      is stored as:                                                                          *
 *         A data                                                                              *
 *         A's Virtual Table Pointer                                                           *
 *         B data                                                                              *
 *         X data                                                                              *
 *         [X's Virtual Table Pointer]                                                         *
 *         C data                                                                              *
 *                                                                                             *
 *      and                                                                                    *
 *         class A                                                                             *
 *         class B: public A                                                                   *
 *         class C: public X, B                                                                *
 *      is stored in memory as:                                                                *
 *         X data                                                                              *
 *         [X's Virtual Table Pointer]                                                         *
 *         A data                                                                              *
 *         A's Virtual Table Pointer                                                           *
 *         B data                                                                              *
 *         C data                                                                              *
 *                                                                                             *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/10/1995 BR : Created.                                                                  *
 *=============================================================================================*/
bool Read_Object(void *ptr, int base_size, int class_size, FileClass & file, void * vtable)
{
	int size;					// object size in bytes

	/*
	**	Read size of this chunk.
	*/
	if (file.Read(&size,sizeof(size)) != sizeof(size)) {
		return(false);
	}

	/*
	**	Error if incorrect size.
	*/
	if (size != class_size) {
		return(false);
	}

	/*
	**	Read object data.
	*/
	if (file.Read(ptr, class_size) != (class_size)) {
		return(false);
	}

	/*
	**	Fill in VTable.
	**	Original code: vtable at base_size - 4 (Watcom/32-bit layout).
	**	GCC/Clang LP64: vtable pointer is at offset 0 of the object.
	*/
	if (vtable) {
		((void **)ptr)[0] = vtable;
	}

	return(true);
}


/***********************************************************************************************
 * Write_Object -- reads an object from disk, in a safe way                                    *
 *                                                                                             *
 * This routine writes an object in 2 pieces, skipping the embedded                            *
 * virtual function table pointer.                                                             *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      ptr            pointer to object to write                                              *
 *      class_size      size of the class itself                                               *
 *      file            file to use for I/O                                                    *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      true = OK, false = error                                                               *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      This routine ASSUMES the program modules are compiled with:                            *
 *      -Vb-      Always make the virtual function table ptr 2 bytes long                      *
 *      -Vt      Put the virtual function table after the 1st class's data                     *
 *                                                                                             *
 *    Also see warnings for Read_Object().                                                     *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/10/1995 BR : Created.                                                                  *
 *=============================================================================================*/
bool Write_Object(void *ptr, int class_size, FileClass & file)
{
	/*
	**	Save size of this chunk.
	*/
	int wrote = file.Write(&class_size,sizeof(class_size));
	if (wrote != sizeof(class_size)) {
		fprintf(stderr, "Write_Object: size write failed (wrote %d, expected %zu)\n", wrote, sizeof(class_size));
		return(false);
	}

	/*
	**	Save object data.
	*/
	wrote = file.Write(ptr, class_size);
	if (wrote != class_size) {
		fprintf(stderr, "Write_Object: data write failed (wrote %d, expected %d)\n", wrote, class_size);
		return(false);
	}

	return(true);
}


/***************************************************************************
 * Get_Savefile_Info -- gets description, scenario #, house                *
 *                                                                         *
 * INPUT:                                                                  *
 *      id         numerical ID, for the file extension                    *
 *      buf      buffer to store description in                            *
 *      scenp      ptr to variable to hold scenario                        *
 *      housep   ptr to variable to hold house                             *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      true = OK, false = error (save-game file invalid)                  *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/12/1995 BR : Created.                                              *
 *=========================================================================*/
bool Get_Savefile_Info(int id, char *buf, unsigned *scenp, HousesType *housep)
{
	RawFileClass file;
	char name[_MAX_FNAME+_MAX_EXT];
	unsigned long version;
	char descr_buf[DESCRIP_MAX];

	/*
	**	Generate the filename to load
	*/
	snprintf(name, sizeof(name), "%sSAVEGAME.%03d", TD_Get_Save_Path(), id);
	fprintf(stderr, "Get_Savefile_Info: trying '%s'\n", name);

	/*
	**	If the file opens OK, read the file
	*/
	if (file.Open(name, READ)) {
		fprintf(stderr, "Get_Savefile_Info: file opened OK\n");

		/*
		**	Read in the description, scenario #, and the house
		*/
		if (file.Read(descr_buf, DESCRIP_MAX) != DESCRIP_MAX) {
			fprintf(stderr, "Get_Savefile_Info: FAILED reading description\n");
			file.Close();
			return(false);
		}

		descr_buf[strlen(descr_buf) - 2] = '\0';	// trim off CR/LF
		strcpy(buf, descr_buf);
		fprintf(stderr, "Get_Savefile_Info: descr='%s'\n", buf);

		if (file.Read(scenp, sizeof(unsigned)) != sizeof(unsigned)) {
			fprintf(stderr, "Get_Savefile_Info: FAILED reading scenario\n");
			file.Close();
			return(false);
		}
		fprintf(stderr, "Get_Savefile_Info: scenario=%u\n", *scenp);

		if (file.Read(housep, sizeof(HousesType)) != sizeof(HousesType)) {
			fprintf(stderr, "Get_Savefile_Info: FAILED reading house\n");
			file.Close();
			return(false);
		}
		fprintf(stderr, "Get_Savefile_Info: house=%d\n", (int)*housep);

		/*
		**	Read & verify the save-game version #
		*/
		if (file.Read(&version,sizeof(version)) != sizeof(version)) {
			fprintf(stderr, "Get_Savefile_Info: FAILED reading version\n");
			file.Close();
			return(false);
		}

		fprintf(stderr, "Get_Savefile_Info: file_version=0x%lx, expected=0x%lx, sizeof(ulong)=%zu\n",
			version, (unsigned long)SAVEGAME_VERSION, sizeof(unsigned long));

		if (version!=SAVEGAME_VERSION) {
			fprintf(stderr, "Get_Savefile_Info: VERSION MISMATCH!\n");
			file.Close();
			return(false);
		}

		file.Close();
		fprintf(stderr, "Get_Savefile_Info: SUCCESS\n");

		return(true);
	}
	fprintf(stderr, "Get_Savefile_Info: FAILED to open file\n");
	return(false);
}


/***************************************************************************
 * TechnoType_To_Target -- converts TechnoTypeClass to TARGET              *
 *                                                                         *
 * INPUT:                                                                  *
 *      ptr      pointer to convert                                        *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      target value                                                       *
 *                                                                         *
 * WARNINGS:                                                               *
 *      Be certain that you only use the returned target value by passing  *
 *      it to Target_To_TechnoType; do NOT call As_Techno, or you'll get   *
 *      a totally invalid pointer.                                         *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/12/1995 BR : Created.                                              *
 *=========================================================================*/
TARGET TechnoType_To_Target(TechnoTypeClass const * ptr)
{
	TARGET target;

	switch (ptr->What_Am_I()) {
		case RTTI_INFANTRYTYPE:
			target = Build_Target(KIND_INFANTRY, ((InfantryTypeClass const *)ptr)->Type);
			break;

		case RTTI_UNITTYPE:
			target = Build_Target(KIND_UNIT, ((UnitTypeClass const *)ptr)->Type);
			break;

		case RTTI_AIRCRAFTTYPE:
			target = Build_Target(KIND_AIRCRAFT, ((AircraftTypeClass const *)ptr)->Type);
			break;

		case RTTI_BUILDINGTYPE:
			target = Build_Target(KIND_BUILDING, ((BuildingTypeClass const *)ptr)->Type);
			break;

		default:
			target = 0;
			break;
	}

	return(target);
}


/***************************************************************************
 * Target_To_TechnoType -- converts TARGET to TechnoTypeClass              *
 *                                                                         *
 * INPUT:                                                                  *
 *      target      TARGET value to convert                                *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      pointer to the TechnoTypeClass for this target value               *
 *                                                                         *
 * WARNINGS:                                                               *
 *      The TARGET value MUST have been generated with TechnoType_To_Target;*
 *      If you give this routine a target generated by an As_Target()      *
 *      routine, it will return a bogus pointer.                           *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/12/1995 BR : Created.                                              *
 *=========================================================================*/
TechnoTypeClass const * Target_To_TechnoType(TARGET target)
{
	switch (Target_Kind(target)) {
		case KIND_INFANTRY:
			return(&InfantryTypeClass::As_Reference((InfantryType)Target_Value(target)));

		case KIND_UNIT:
			return(&UnitTypeClass::As_Reference((UnitType)Target_Value(target)));

		case KIND_AIRCRAFT:
			return(&AircraftTypeClass::As_Reference((AircraftType)Target_Value(target)));

		case KIND_BUILDING:
			return(&BuildingTypeClass::As_Reference((StructType)Target_Value(target)));
	}
	return(NULL);
}


/***************************************************************************
 * Get_VTable -- gets the VTable pointer for the given object              *
 *                                                                         *
 * INPUT:                                                                  *
 *      ptr      pointer to check                                          *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      none                                                               *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/12/1995 BR : Created.                                              *
 *=========================================================================*/
void * Get_VTable(void *ptr, int base_size)
{
	return(((void **)(((char *)ptr) + base_size - 4))[0]);
}


/***************************************************************************
 * Set_VTable -- sets the VTable pointer for the given object              *
 *                                                                         *
 * INPUT:                                                                  *
 *      ptr         pointer to check                                       *
 *      base_size   size of base class                                     *
 *      vtable      value of VTable to plug in                             *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      none                                                               *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/12/1995 BR : Created.                                              *
 *=========================================================================*/
void Set_VTable(void *ptr, int base_size, void *vtable)
{
	((void **)(((char *)ptr) + base_size - 4))[0] = vtable;
}


#if 0
/****************************************************************************
Dump routine: prints everything about everything related to the Save/Load
process (OK, not exactly everything, but lots of stuff)
****************************************************************************/
void Dump(void)
{
	int i,j;
	FILE *fp;
	char *layername[] = {
		"Ground",
		"Air",
		"Top"
	};

	/*
	------------------------------- Open file --------------------------------
	*/
	fp = fopen("dump.txt","wt");

	/*
	------------------------------ Logic Layer -------------------------------
	*/
	fprintf(fp,"--------------------- Logic Layer ---------------------\n");
	fprintf(fp,"Count: %d\n",Logic.Count());
	for (j = 0; j < Logic.Count(); j++) {
		fprintf(fp, "Entry %d: %x \n",j,Logic[j]);
	}
	fprintf(fp,"\n");

	/*
	------------------------------- Map Layers -------------------------------
	*/
	for (i = 0; i < LAYER_COUNT; i++) {
		fprintf(fp,"----------------- Map Layer %s ---------------------\n",
			layername[i]);
		fprintf(fp,"Count: %d\n",Map.Layer[i].Count());
		for (j = 0; j < Map.Layer[i].Count(); j++) {
			fprintf(fp, "Entry %d: %x \n",j,Map.Layer[i][j]);
		}
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ TeamTypes --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",TeamTypes.ActiveCount);
	for (i = 0; i < TEAMTYPE_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d Name:%s\n",i,TeamTypes[i].IsActive,
			TeamTypes[i].Get_Name());
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Teams --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Teams.ActiveCount);
	for (i = 0; i < TEAM_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d Name:%s\n",i,Teams[i].IsActive,
			Teams[i].Class->Get_Name());
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Triggers --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Triggers.ActiveCount);
	for (i = 0; i < TRIGGER_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d Name:%s\n",i,Triggers[i].IsActive,
			Triggers[i].Get_Name());
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Aircraft --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Aircraft.ActiveCount);
	for (i = 0; i < AIRCRAFT_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Aircraft[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Anims --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Anims.ActiveCount);
	for (i = 0; i < ANIM_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Anims[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Buildings --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Buildings.ActiveCount);
	for (i = 0; i < BUILDING_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Buildings[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Bullets --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Bullets.ActiveCount);
	for (i = 0; i < BULLET_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Bullets[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Infantry --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Infantry.ActiveCount);
	for (i = 0; i < INFANTRY_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Infantry[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Overlays --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Overlays.ActiveCount);
	for (i = 0; i < OVERLAY_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Overlays[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Reinforcements --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Reinforcements.ActiveCount);
	for (i = 0; i < REINFORCEMENT_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Reinforcements[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Smudges --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Smudges.ActiveCount);
	for (i = 0; i < SMUDGE_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Smudges[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Templates --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Templates.ActiveCount);
	for (i = 0; i < TEMPLATE_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Templates[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Terrains --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Terrains.ActiveCount);
	for (i = 0; i < TERRAIN_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Terrains[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Units --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Units.ActiveCount);
	for (i = 0; i < UNIT_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Units[i].IsActive);
	}
	fprintf(fp,"\n");

	fprintf(fp,"------------------ Factories --------------------------\n");
	fprintf(fp,"ActiveCount: %d\n",Factories.ActiveCount);
	for (i = 0; i < FACTORY_MAX; i++) {
		fprintf(fp,"Entry %d: Active:%d \n",i,Factories[i].IsActive);
	}
	fprintf(fp,"\n");

	fclose(fp);

	/*
	---------------------------- Flush the cache -----------------------------
	*/
	fp = fopen("dummy.bin","wt");
	for (i = 0; i < 100; i++) {
		fprintf(fp,"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
	}
	fclose(fp);
}
#endif

