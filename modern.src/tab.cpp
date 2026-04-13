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

/* $Header:   F:\projects\c&c\vcs\code\tab.cpv   2.18   16 Oct 1995 16:52:04   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : TAB.CPP                                                      *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 12/15/94                                                     *
 *                                                                                             *
 *                  Last Update : August 25, 1995 [JLB]                                        *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   TabClass::AI -- Handles player I/O with the tab buttons.                                  *
 *   TabClass::Draw_It -- Displays the tab buttons as necessary.                               *
 *   TabClass::Set_Active -- Activates a "filefolder tab" button.                              *
 *   TabClass::TabClass -- Default construct for the tab button class.                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "function.h"

#ifdef USE_RENDER_BRIDGE
extern bool Render_Bridge_Use_Legacy_Sidebar_Mode();
extern void Render_Bridge_Get_Logical_Screen_Size(int& w, int& h);

static void convert_legacy_ui_mouse_for_tab(int& x, int& y)
{
	if (!Render_Bridge_Use_Legacy_Sidebar_Mode()) {
		return;
	}

	int base_w = SeenBuff.Get_Width();
	int base_h = SeenBuff.Get_Height();
	int logical_w = 0;
	int logical_h = 0;
	Render_Bridge_Get_Logical_Screen_Size(logical_w, logical_h);
	if (base_w <= 0 || base_h <= 0 || logical_w <= 0 || logical_h <= 0) {
		return;
	}

	float scale_x = static_cast<float>(logical_w) / static_cast<float>(base_w);
	float scale_y = static_cast<float>(logical_h) / static_cast<float>(base_h);
	float scale = (scale_x < scale_y) ? scale_x : scale_y;
	if (scale <= 0.0f) {
		return;
	}

	int offset_x = static_cast<int>((logical_w - base_w * scale) * 0.5f);
	int offset_y = static_cast<int>((logical_h - base_h * scale) * 0.5f);

	x = static_cast<int>((static_cast<float>(x - offset_x)) / scale);
	y = static_cast<int>((static_cast<float>(y - offset_y)) / scale);
}
#endif


void const * TabClass::TabShape = NULL;


/***********************************************************************************************
 * TabClass::TabClass -- Default construct for the tab button class.                           *
 *                                                                                             *
 *    The default constructor merely sets the tab buttons to default non-selected state.       *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/15/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
TabClass::TabClass(void)
{
	IsToRedraw = false;
//	Select = -1;
}


/***********************************************************************************************
 * TabClass::Draw_It -- Displays the tab buttons as necessary.                                 *
 *                                                                                             *
 *    This routine is called whenever the display is being redrawn (in some fashion). The      *
 *    parameter can be used to force the tab buttons to redraw completely. The default action  *
 *    is to only redraw if the tab buttons have been explicitly flagged to be redraw. The      *
 *    result of this is the elimination of unnecessary redraws.                                *
 *                                                                                             *
 * INPUT:   complete -- bool; Force redraw of the entire tab button graphics?                  *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/15/1994 JLB : Created.                                                                 *
 *   05/19/1995 JLB : New EVA style.                                                           *
 *=============================================================================================*/
void TabClass::Draw_It(bool complete)
{
	//DBG("TabClass::Draw_It");

	SidebarClass::Draw_It(complete);

	if (Debug_Map){
		//HidPage.Unlock();
		return;
	}

	/*
	**	Redraw the top bar imagery if flagged to do so or if the entire display needs
	**	to be redrawn.
	*/
	int width  = SeenBuff.Get_Width();
	int rightx = width - 1;

	if (complete || IsToRedraw) {

		if (LogicPage->Lock()){

			LogicPage->Fill_Rect(0, 0, rightx, Tab_Height-2, BLACK);
			CC_Draw_Shape(TabShape, 0, 0, 0, WINDOW_MAIN, SHAPE_NORMAL);
			CC_Draw_Shape(TabShape, 0, width-Eva_Width, 0, WINDOW_MAIN, SHAPE_NORMAL);
			Draw_Credits_Tab();
			LogicPage->Draw_Line(0, Tab_Height-1, rightx, Tab_Height-1, BLACK);

			Fancy_Text_Print(TXT_TAB_BUTTON_CONTROLS, Eva_Width/2, 0, 11, TBLACK, TPF_GREEN12_GRAD|TPF_CENTER | TPF_USE_GRAD_PAL);
			Fancy_Text_Print(TXT_TAB_SIDEBAR, width-(Eva_Width/2), 0, 11, TBLACK, TPF_GREEN12_GRAD|TPF_CENTER | TPF_USE_GRAD_PAL);
		}
		LogicPage->Unlock();
	}

	Credits.Graphic_Logic(complete || IsToRedraw);
	IsToRedraw = false;
}


void TabClass::Draw_Credits_Tab(void)
{
	int factor = Get_Resolution_Factor();
	int eva_w = 80 * (factor + 1);
	int xx = SeenBuff.Get_Width() - (120 << factor);
	CC_Draw_Shape(TabShape, 0, xx - eva_w/2, 0, WINDOW_MAIN, SHAPE_NORMAL);
}


/***********************************************************************************************
 * TC::Hilite_Tab -- Draw a tab in its depressed state                                         *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Tab to draw (not used)                                                            *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    5/21/96 10:47AM ST : Created                                                              *
 *=============================================================================================*/

void TabClass::Hilite_Tab(int tab)
{
	int xpos = 0;
	int text = TXT_TAB_BUTTON_CONTROLS;
	tab = tab;

	CC_Draw_Shape(TabShape, 1 , xpos, 0, WINDOW_MAIN, SHAPE_NORMAL);
	Fancy_Text_Print(TXT_TAB_BUTTON_CONTROLS, 80, 0, 11, TBLACK, TPF_GREEN12|TPF_CENTER | TPF_USE_GRAD_PAL);
}


/***********************************************************************************************
 * TabClass::AI -- Handles player I/O with the tab buttons.                                    *
 *                                                                                             *
 *    This routine is called every game tick and passed whatever key the player has supplied.  *
 *    If the input selects a tab button, then the graphic gets updated accordingly.            *
 *                                                                                             *
 * INPUT:   input -- The player's input character (might be mouse click).                      *
 *                                                                                             *
 *          x,y   -- Mouse coordinates at time of input.                                       *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/15/1994 JLB : Created.                                                                 *
 *   12/31/1994 JLB : Uses mouse coordinate parameters.                                        *
 *   05/31/1995 JLB : Fixed to handle mouse shape properly.                                    *
 *   08/25/1995 JLB : Handles new scrolling option.                                            *
 *=============================================================================================*/
void TabClass::AI(KeyNumType &input, int x, int y)
{
	int ui_x = x;
	int ui_y = y;
#ifdef USE_RENDER_BRIDGE
	convert_legacy_ui_mouse_for_tab(ui_x, ui_y);
#endif

	if (ui_y >= 0 && ui_y < Tab_Height && ui_x < (SeenBuff.Get_Width() - 1) && ui_x > 0) {

		bool ok = false;
		int	width = SeenBuff.Get_Width();

		/*
		**	If the mouse is at the top of the screen, then the tab bars only work
		**	in certain areas. If the special scroll modification is not active, then
		**	the tabs never work when the mouse is at the top of the screen.
		*/
		if (ui_y > 0 || (Special.IsScrollMod && ((ui_x > 3 && ui_x < Eva_Width) || (ui_x < width-3 && ui_x > width-Eva_Width)))) {
			ok = true;
		}

		if (ok) {
			if (input == KN_LMOUSE) {
				int sel = -1;
				if (ui_x < Eva_Width) sel = 0;
				if (ui_x > width-Eva_Width) sel = 1;
				if (sel >= 0) {
					Set_Active(sel);
					input = KN_NONE;
				}
			}

			Override_Mouse_Shape(MOUSE_NORMAL, false);
		}
	}

	Credits.AI();

	//DBG("TabClass::AI → SidebarClass::AI");
	SidebarClass::AI(input, ui_x, ui_y);
}


/***********************************************************************************************
 * TabClass::Set_Active -- Activates a "filefolder tab" button.                                *
 *                                                                                             *
 *    This function is used to activate one of the file folder tab buttons that appear at the  *
 *    top edge of the screen.                                                                  *
 *                                                                                             *
 * INPUT:   select   -- The button to activate. 0 = left button, 1=next button, etc.           *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/15/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
 void TabClass::Set_Active(int select)
{
	switch (select) {
		case 0:
			Queue_Options();
			break;

		case 1:
			Map.SidebarClass::Activate(-1);
			break;

		default:
			break;
	}
}

void TabClass::One_Time(void)
{
	int factor = (SeenBuff.Get_Width() == 320) ? 1 : 2;
	Eva_Width	= 80 * factor;
	Tab_Height	= 8 * factor;

	SidebarClass::One_Time();
	TabShape = Hires_Retrieve("TABS.SHP");
}
