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

/* $Header:   F:\projects\c&c\vcs\code\toggle.cpv   2.18   16 Oct 1995 16:50:56   JOE_BOSTIC  $ */
/*********************************************************************************************** 
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               *** 
 *********************************************************************************************** 
 *                                                                                             * 
 *                 Project Name : Command & Conquer                                            * 
 *                                                                                             * 
 *                    File Name : TOGGLE.CPP                                                   * 
 *                                                                                             * 
 *                   Programmer : Joe L. Bostic                                                * 
 *                                                                                             * 
 *                   Start Date : 01/15/95                                                     * 
 *                                                                                             * 
 *                  Last Update : February 2, 1995 [JLB]                                       * 
 *                                                                                             * 
 *---------------------------------------------------------------------------------------------* 
 * Functions:                                                                                  * 
 *   ToggleClass::ToggleClass -- Normal constructor for toggle button gadgets.                 * 
 *   ToggleClass::Turn_Off -- Turns the toggle button to the "OFF" state.                      * 
 *   ToggleClass::Turn_On -- Turns the toggle button to the "ON" state.                        * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "function.h"
#include "toggle.h"

#ifdef USE_RENDER_BRIDGE
extern bool Render_Bridge_Use_Legacy_Sidebar_Mode();
extern void Render_Bridge_Get_Logical_Screen_Size(int& w, int& h);

static bool toggle_is_outside_legacy_tactical(int x, int y, int w, int h)
{
	int tac_x = Map.TacPixelX;
	int tac_y = Map.TacPixelY;
	int tac_w = WindowList[WINDOW_TACTICAL][WINDOWWIDTH] << 3;
	int tac_h = WindowList[WINDOW_TACTICAL][WINDOWHEIGHT];

	int gx2 = x + w;
	int gy2 = y + h;
	int tx2 = tac_x + tac_w;
	int ty2 = tac_y + tac_h;

	bool intersects =
		(x < tx2) &&
		(gx2 > tac_x) &&
		(y < ty2) &&
		(gy2 > tac_y);

	return !intersects;
}

static void convert_legacy_ui_mouse_for_toggle(int gadget_x, int gadget_y, int gadget_w, int gadget_h,
	int& mousex, int& mousey)
{
	if (!Render_Bridge_Use_Legacy_Sidebar_Mode()) {
		return;
	}
	if (!toggle_is_outside_legacy_tactical(gadget_x, gadget_y, gadget_w, gadget_h)) {
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

	mousex = static_cast<int>((static_cast<float>(mousex - offset_x)) / scale);
	mousey = static_cast<int>((static_cast<float>(mousey - offset_y)) / scale);
}
#endif


/*********************************************************************************************** 
 * ToggleClass::ToggleClass -- Normal constructor for toggle button gadgets.                   * 
 *                                                                                             * 
 *    This is the normal constructor for toggle buttons. A toggle button is one that most      * 
 *    closesly resembles the Windows style. It has an up and down state as well as an on       * 
 *    and off state.                                                                           * 
 *                                                                                             * 
 * INPUT:   id    -- ID number for this button.                                                * 
 *                                                                                             * 
 *          x,y   -- Pixel coordinate of upper left corner of this button.                     * 
 *                                                                                             * 
 *          w,h   -- Width and height of the button.                                           * 
 *                                                                                             * 
 * OUTPUT:  none                                                                               * 
 *                                                                                             * 
 * WARNINGS:   none                                                                            * 
 *                                                                                             * 
 * HISTORY:                                                                                    * 
 *   01/15/1995 JLB : Created.                                                                 * 
 *=============================================================================================*/
ToggleClass::ToggleClass(unsigned id, int x, int y, int w, int h) 
	: ControlClass(id, x, y, w, h, LEFTPRESS|LEFTRELEASE, true)
{
	IsPressed = false;
	IsOn = false;
	IsToggleType = false;
}	


/*********************************************************************************************** 
 * ToggleClass::Turn_On -- Turns the toggle button to the "ON" state.                          * 
 *                                                                                             * 
 *    This routine will turn the button on. The button will also be flagged to be redrawn      * 
 *    at the next opportunity.                                                                 * 
 *                                                                                             * 
 * INPUT:   none                                                                               * 
 *                                                                                             * 
 * OUTPUT:  none                                                                               * 
 *                                                                                             * 
 * WARNINGS:   none                                                                            * 
 *                                                                                             * 
 * HISTORY:                                                                                    * 
 *   01/15/1995 JLB : Created.                                                                 * 
 *=============================================================================================*/
void ToggleClass::Turn_On(void)
{
	IsOn = true;
	Flag_To_Redraw();
}	


/*********************************************************************************************** 
 * ToggleClass::Turn_Off -- Turns the toggle button to the "OFF" state.                        * 
 *                                                                                             * 
 *    This routine will turn the toggle button "off". It will also be flagged to be redrawn    * 
 *    at the next opportunity.                                                                 * 
 *                                                                                             * 
 * INPUT:   none                                                                               * 
 *                                                                                             * 
 * OUTPUT:  none                                                                               * 
 *                                                                                             * 
 * WARNINGS:   none                                                                            * 
 *                                                                                             * 
 * HISTORY:                                                                                    * 
 *   01/15/1995 JLB : Created.                                                                 * 
 *=============================================================================================*/
void ToggleClass::Turn_Off(void)
{
	IsOn = false;	
	Flag_To_Redraw();
}	


/*********************************************************************************************** 
 * ToggleClass::Action -- Handles mouse clicks on a text button.                               *
 *                                                                                             * 
 *    This routine will process any mouse or keyboard event that is associated with this       * 
 *    button object. It detects and flags the text button so that it will properly be drawn    * 
 *    in a pressed or raised state. It also handles any toggle state for the button.           * 
 *                                                                                             * 
 * INPUT:   flags -- The event flags that triggered this button.                               * 
 *                                                                                             * 
 *          key   -- The keyboard code associated with this event. Usually this is KN_LMOUSE   * 
 *                   or similar, but it could be a regular key if this text button is given    * 
 *                   a hotkey.                                                                 * 
 *                                                                                             * 
 * OUTPUT:  Returns whatever the lower level processing for buttons decides. This is usually   * 
 *          true.                                                                              * 
 *                                                                                             * 
 * WARNINGS:   The button is flagged to be redrawn by this routine.                            * 
 *                                                                                             * 
 * HISTORY:                                                                                    * 
 *   01/14/1995 JLB : Created.                                                                 * 
 *   02/02/1995 JLB : Left press doesn't get passed to other buttons now                       * 
 *=============================================================================================*/
int ToggleClass::Action(unsigned flags, KeyNumType &key)
{
	/*
	**	If there are no action flag bits set, then this must be a forced call. A forced call
	**	must never actually function like a real call, but rather only performs any necessary
	**	graphic updating.
	*/
	if (!flags) {
		int mousex = Get_Mouse_X();
		int mousey = Get_Mouse_Y();
#ifdef USE_RENDER_BRIDGE
		convert_legacy_ui_mouse_for_toggle(X, Y, Width, Height, mousex, mousey);
#endif
		if ((unsigned)(mousex - X) < Width && (unsigned)(mousey - Y) < Height ) {
			if (!IsPressed) {
				IsPressed = true;
				Flag_To_Redraw();
			}
		} else {
			if (IsPressed) {
				IsPressed = false;
				Flag_To_Redraw();
			}
		}
	} 

	/*
	**	Handle the sticky state for this gadget. It must be processed here
	**	because the event flags might be cleared before the action function
	**	is called.
	*/
	Sticky_Process(flags);

	/*
	**	Flag the button to show the pressed down imagery if this mouse button
	**	was pressed over this gadget.
	*/
	if (flags & LEFTPRESS) {
		IsPressed = true;
		Flag_To_Redraw();
		flags &= ~LEFTPRESS;
		ControlClass::Action(flags, key);
		key = KN_NONE;				// erase the event
		return(true);		// stop processing other buttons now
	}

	if (flags & LEFTRELEASE) {
		if (IsPressed) {
			if (IsToggleType) {
				IsOn = (IsOn == false);
			}
			IsPressed = false;
		} else {
			flags &= ~LEFTRELEASE;
		}
	}

	/*
	**	Do normal button processing. This ends up causing the button's ID number to
	**	be returned from the controlling Input() function.
	*/
	return(ControlClass::Action(flags, key));
}	

