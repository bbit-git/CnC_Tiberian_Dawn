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

/* $Header:   F:\projects\c&c\vcs\code\special.cpv   1.4   16 Oct 1995 16:50:06   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : SPECIAL.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 05/27/95                                                     *
 *                                                                                             *
 *                  Last Update : May 27, 1995 [JLB]                                           *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "function.h"
#ifdef USE_RENDER_BRIDGE
#include "render_bridge.h"
#endif

#define	OPTION_WIDTH	236
#define	OPTION_HEIGHT	162
#define	OPTION_X			((320 - OPTION_WIDTH) / 2)
#define	OPTION_Y			(200 - OPTION_HEIGHT) / 2

void Special_Dialog(void)
{
#ifdef USE_RENDER_BRIDGE
	{
		SpecialClass oldspecial = Special;
		int screen_w = SeenBuff.Get_Width();
		int screen_h = SeenBuff.Get_Height();
		{ extern void Render_Bridge_Get_Logical_Screen_Size(int& w, int& h);
		  Render_Bridge_Get_Logical_Screen_Size(screen_w, screen_h); }

		bool opts[10];
		const char* labels[10];
		opts[0] = Special.IsSeparate;       labels[0] = Text_String(TXT_SEPARATE_HELIPAD);
		opts[1] = Special.IsVisibleTarget;  labels[1] = Text_String(TXT_VISIBLE_TARGET);
		opts[2] = Special.IsTreeTarget;     labels[2] = Text_String(TXT_TREE_TARGET);
		opts[3] = Special.IsMCVDeploy;      labels[3] = Text_String(TXT_MCV_DEPLOY);
		opts[4] = Special.IsSmartDefense;   labels[4] = Text_String(TXT_SMART_DEFENCE);
		opts[5] = Special.IsThreePoint;     labels[5] = Text_String(TXT_THREE_POINT);
		opts[6] = Special.IsTFast;          labels[6] = Text_String(TXT_TIBERIUM_FAST);
		opts[7] = Special.IsRoad;           labels[7] = Text_String(TXT_ROAD_PIECES);
		opts[8] = Special.IsScatter;        labels[8] = Text_String(TXT_SCATTER);
		opts[9] = Special.IsNamed;          labels[9] = Text_String(TXT_SHOW_NAMES);

		Render_Bridge_UI_Set_Special_Dialog(opts, labels, screen_w, screen_h,
		                                    Text_String(TXT_SPECIAL_OPTIONS),
		                                    Text_String(TXT_OK),
		                                    Text_String(TXT_CANCEL));

		while (Render_Bridge_UI_Get_Special_Dialog_Result() == 0) {
			if (Main_Loop()) {
				Render_Bridge_UI_Clear_Special_Dialog();
				goto special_cleanup;
			}
		}

		if (Render_Bridge_UI_Get_Special_Dialog_Result() == 1) {
			bool result[10];
			Render_Bridge_UI_Get_Special_Dialog_State(result);
			oldspecial.IsSeparate       = result[0];
			oldspecial.IsVisibleTarget  = result[1];
			oldspecial.IsTreeTarget     = result[2];
			oldspecial.IsMCVDeploy      = result[3];
			oldspecial.IsSmartDefense   = result[4];
			oldspecial.IsThreePoint     = result[5];
			oldspecial.IsTFast          = result[6];
			oldspecial.IsRoad           = result[7];
			oldspecial.IsScatter        = result[8];
			oldspecial.IsNamed          = result[9];
			OutList.Add(EventClass(oldspecial));
		}

		special_cleanup:
		HiddenPage.Clear();
		Map.Flag_To_Redraw(true);
		Map.Render();
		return;
	}
#else
	SpecialClass oldspecial = Special;
	GadgetClass * buttons = NULL;
	static struct {
		int Description;
		int Setting;
		CheckBoxClass * Button;
	} _options[] = {
//		{TXT_DEFENDER_ADVANTAGE, 0, 0},
		{TXT_SEPARATE_HELIPAD, 0, 0},
		{TXT_VISIBLE_TARGET, 0, 0},
		{TXT_TREE_TARGET, 0, 0},
		{TXT_MCV_DEPLOY, 0, 0},
		{TXT_SMART_DEFENCE, 0, 0},
		{TXT_THREE_POINT, 0, 0},
//		{TXT_TIBERIUM_GROWTH, 0, 0},
//		{TXT_TIBERIUM_SPREAD, 0, 0},
		{TXT_TIBERIUM_FAST, 0, 0},
		{TXT_ROAD_PIECES, 0, 0},
		{TXT_SCATTER, 0, 0},
		{TXT_SHOW_NAMES, 0, 0},
	};

	TextButtonClass ok(200, TXT_OK, TPF_6PT_GRAD|TPF_NOSHADOW, OPTION_X+5, OPTION_Y+OPTION_HEIGHT-15);
	TextButtonClass cancel(201, TXT_CANCEL, TPF_6PT_GRAD|TPF_NOSHADOW, OPTION_X+OPTION_WIDTH-50, OPTION_Y+OPTION_HEIGHT-15);
	buttons = &ok;
	cancel.Add(*buttons);

	int idx;
	for (idx = 0; idx < sizeof(_options)/sizeof(_options[0]); idx++) {
		_options[idx].Button = new CheckBoxClass(100+idx, OPTION_X+7, OPTION_Y+20+(idx*10));
		if (_options[idx].Button) {
			_options[idx].Button->Add(*buttons);

			bool value = false;
			switch (_options[idx].Description) {
				case TXT_SEPARATE_HELIPAD:
					value = Special.IsSeparate;
					break;

				case TXT_SHOW_NAMES:
					value = Special.IsNamed;
					break;

				case TXT_DEFENDER_ADVANTAGE:
					value = Special.IsDefenderAdvantage;
					break;

				case TXT_VISIBLE_TARGET:
					value = Special.IsVisibleTarget;
					break;

				case TXT_TREE_TARGET:
					value = Special.IsTreeTarget;
					break;

				case TXT_MCV_DEPLOY:
					value = Special.IsMCVDeploy;
					break;

				case TXT_SMART_DEFENCE:
					value = Special.IsSmartDefense;
					break;

				case TXT_THREE_POINT:
					value = Special.IsThreePoint;
					break;

				case TXT_TIBERIUM_GROWTH:
					value = Special.IsTGrowth;
					break;

				case TXT_TIBERIUM_SPREAD:
					value = Special.IsTSpread;
					break;

				case TXT_TIBERIUM_FAST:
					value = Special.IsTFast;
					break;

				case TXT_ROAD_PIECES:
					value = Special.IsRoad;
					break;

				case TXT_SCATTER:
					value = Special.IsScatter;
					break;
			}

			_options[idx].Setting = value;
			if (value) {
				_options[idx].Button->Turn_On();
			} else {
				_options[idx].Button->Turn_Off();
			}
		}
	}

	Map.Override_Mouse_Shape(MOUSE_NORMAL);
	Set_Logic_Page(SeenBuff);
	bool recalc = true;
	bool display = true;
	bool process = true;
	while (process) {

		/*
		** If we have just received input focus again after running in the background then
		** we need to redraw.
		*/
		if (AllSurfaces.SurfacesRestored){
			AllSurfaces.SurfacesRestored=FALSE;
			display=TRUE;
		}

		if (GameToPlay == GAME_NORMAL) {
			Call_Back();
		} else {
			if (Main_Loop()) {
				process = false;
			}
		}

		if (display) {
			display = false;

			Hide_Mouse();
			Dialog_Box(OPTION_X, OPTION_Y, OPTION_WIDTH, OPTION_HEIGHT);
			Draw_Caption(TXT_SPECIAL_OPTIONS, OPTION_X, OPTION_Y, OPTION_WIDTH);

			for (idx = 0; idx < sizeof(_options)/sizeof(_options[0]); idx++) {
				Fancy_Text_Print(_options[idx].Description, _options[idx].Button->X+10, _options[idx].Button->Y, CC_GREEN, TBLACK, TPF_6PT_GRAD|TPF_USE_GRAD_PAL|TPF_NOSHADOW);
			}
			buttons->Draw_All();
			Show_Mouse();
		}

		KeyNumType input = buttons->Input();
		switch (input) {
			case KN_ESC:
			case 200|KN_BUTTON:
				process = false;
				for (idx = 0; idx < sizeof(_options)/sizeof(_options[0]); idx++) {
					switch (_options[idx].Description) {
						case TXT_SEPARATE_HELIPAD:
							oldspecial.IsSeparate = _options[idx].Setting;
							break;

						case TXT_SHOW_NAMES:
							oldspecial.IsNamed = _options[idx].Setting;
							break;

						case TXT_DEFENDER_ADVANTAGE:
							oldspecial.IsDefenderAdvantage = _options[idx].Setting;
							break;

						case TXT_VISIBLE_TARGET:
							oldspecial.IsVisibleTarget = _options[idx].Setting;
							break;

						case TXT_TREE_TARGET:
							oldspecial.IsTreeTarget = _options[idx].Setting;
							break;

						case TXT_MCV_DEPLOY:
							oldspecial.IsMCVDeploy = _options[idx].Setting;
							break;

						case TXT_SMART_DEFENCE:
							oldspecial.IsSmartDefense = _options[idx].Setting;
							break;

						case TXT_THREE_POINT:
							oldspecial.IsThreePoint = _options[idx].Setting;
							break;

						case TXT_TIBERIUM_GROWTH:
							oldspecial.IsTGrowth = _options[idx].Setting;
							break;

						case TXT_TIBERIUM_SPREAD:
							oldspecial.IsTSpread = _options[idx].Setting;
							break;

						case TXT_TIBERIUM_FAST:
							oldspecial.IsTFast = _options[idx].Setting;
							break;

						case TXT_ROAD_PIECES:
							oldspecial.IsRoad = _options[idx].Setting;
							break;

						case TXT_SCATTER:
							oldspecial.IsScatter = _options[idx].Setting;
							break;
					}
				}
				OutList.Add(EventClass(oldspecial));
				break;

			case 201|KN_BUTTON:
				process = false;
				break;

			case KN_NONE:
				break;

			default:
				idx = (input & ~KN_BUTTON) - 100;
				if ((unsigned)idx < sizeof(_options)/sizeof(_options[0])) {
					_options[idx].Setting = (_options[idx].Setting == false);
					if (_options[idx].Setting) {
						_options[idx].Button->Turn_On();
					} else {
						_options[idx].Button->Turn_Off();
					}
				}
				break;
		}
	}

	Map.Revert_Mouse_Shape();
	HiddenPage.Clear();
	Map.Flag_To_Redraw(true);
	Map.Render();
#endif // USE_RENDER_BRIDGE
}

