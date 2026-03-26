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

/* $Header:   F:\projects\c&c\vcs\code\mplayer.cpv   1.9   16 Oct 1995 16:51:08   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : MPLAYER.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Bill Randolph                                                *
 *                                                                                             *
 *                   Start Date : April 14, 1995                                               *
 *                                                                                             *
 *                  Last Update : July 5, 1995 [BRR]                                           *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Select_MPlayer_Game -- prompts user for NULL-Modem, Modem, or Network game                *
 *   Read_MultiPlayer_Settings -- reads multi-player settings from conquer.ini                 *
 *   Write_MultiPlayer_Settings -- writes multi-player settings to conquer.ini                 *
 *   Read_Scenario_Descriptions -- reads multi-player scenario #'s # descriptions              *
 *   Free_Scenario_Descriptions -- frees memory for the scenario descriptions                  *
 *   Computer_Message -- "sends" a message from the computer                                   *
 *   Garble_Message -- "garbles" a message                                                     *
 *   Surrender_Dialog -- Prompts user for surrendering                                         *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "function.h"

static void Garble_Message(char *buf);
static void Prepare_Skirmish_Player_State(char const *namebuf, int scenario_index);

int Choose_Internet_Game(void);
int Get_Internet_Host_Or_Join(void);
int Get_IP_Address(void);
void Show_Internet_Connection_Progress(void);

/***********************************************************************************************
 * Select_MPlayer_Game -- prompts user for NULL-Modem, Modem, or Network game                  *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      GAME_NORMAL, GAME_MODEM, etc.                                                          *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
GameType Select_MPlayer_Game (void)
{

	int factor			= (SeenBuff.Get_Width() == 320) ? 1 : 2;
	bool	ipx_avail = FALSE;
	int number_of_buttons;
	/*........................................................................
	Dialog & button dimensions
	........................................................................*/
	int	d_dialog_w = 190*factor;
	int	d_dialog_h = 26*4*factor;
	int 	d_dialog_x = ((SeenBuff.Get_Width() - d_dialog_w) / 2);
//	d_dialog_y = ((200 - d_dialog_h) / 2),
	int	d_dialog_y = ((136*factor - d_dialog_h) / 2);
	int	d_dialog_cx = d_dialog_x + (d_dialog_w / 2);

	int	d_txt6_h = 11 * factor;
	int	d_margin = 7 *factor;

	int	d_modemserial_w = 80*factor;
	int	d_modemserial_h = 9*factor;
	int 	d_modemserial_x = d_dialog_cx - d_modemserial_w / 2;
	int	d_modemserial_y = d_dialog_y + d_margin + d_txt6_h + d_margin;
#if (0)
	int	d_internet_w = 80*factor;
	int	d_internet_h = 9*factor;
	int	d_internet_x = d_dialog_cx - d_internet_w / 2;
	int	d_internet_y = d_modemserial_y + d_modemserial_h + 2*factor;
#endif	//(0)
	int 	d_ipx_w = 80*factor;
	int	d_ipx_h = 9*factor;
	int	d_ipx_x = d_dialog_cx - d_ipx_w / 2;
	int 	d_ipx_y = d_modemserial_y + d_modemserial_h + 2*factor;
//	int 	d_ipx_y = d_internet_y + d_internet_h + 2*factor;

	int	d_cancel_w = 60*factor;
	int 	d_cancel_h = 9*factor;
	int	d_cancel_x = d_dialog_cx - d_cancel_w / 2;
	int	d_cancel_y = d_ipx_y + d_ipx_h + d_margin;

	CountDownTimerClass delay;

	/*........................................................................
	Button enumerations:
	........................................................................*/
	enum {
		BUTTON_MODEMSERIAL = 100,
#if	(0)
		BUTTON_INTERNET,
#endif	//(0)
		BUTTON_IPX,
		BUTTON_CANCEL,

		NUM_OF_BUTTONS = 3,
	};
	number_of_buttons = NUM_OF_BUTTONS;
	/*........................................................................
	Redraw values: in order from "top" to "bottom" layer of the dialog
	........................................................................*/
	typedef enum {
		REDRAW_NONE = 0,
		REDRAW_BUTTONS,		// includes map interior & coord values
		REDRAW_BACKGROUND,	// includes box, map bord, key, coord labels, btns
		REDRAW_ALL = REDRAW_BACKGROUND
	} RedrawType;
	/*........................................................................
	Dialog variables:
	........................................................................*/
	KeyNumType input;								// input from user
	bool process;									// loop while true
	RedrawType display;							// true = re-draw everything
	GameType retval;								// return value
	int selection;
	bool pressed;
	int curbutton;
	TextButtonClass *buttons[NUM_OF_BUTTONS];

	/*........................................................................
	Buttons
	........................................................................*/
	ControlClass *commands = NULL;				// the button list

	//
	// If neither IPX or winsock are active then do only the modem serial dialog
	//
	if (Ipx.Is_IPX()){
		ipx_avail = TRUE;
	}

	/*
	** The first multiplayer button is repurposed for the local skirmish flow.
	*/

	TextButtonClass modemserialbtn (BUTTON_MODEMSERIAL, "Skirmish",
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_modemserial_x, d_modemserial_y, d_modemserial_w, d_modemserial_h);
#if (0)
	TextButtonClass internetbtn (BUTTON_INTERNET, TXT_INTERNET,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_internet_x, d_internet_y, d_internet_w, d_internet_h);
#endif	//(0)
	TextButtonClass ipxbtn (BUTTON_IPX, TXT_NETWORK,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_ipx_x, d_ipx_y, d_ipx_w, d_ipx_h);

	TextButtonClass cancelbtn (BUTTON_CANCEL, TXT_CANCEL,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_cancel_x, d_cancel_y, d_cancel_w, d_cancel_h);

	/*
	------------------------------- Initialize -------------------------------
	*/
	Set_Logic_Page(SeenBuff);

	/*
	............................ Create the list .............................
	*/
	commands = &modemserialbtn;
#if (0)
	internetbtn.Add_Tail(*commands);
#endif	//(0)
	if (ipx_avail){
		ipxbtn.Add_Tail(*commands);
	}
	cancelbtn.Add_Tail(*commands);

	/*
	......................... Fill array of button ptrs ......................
	*/
	curbutton = 0;
	buttons[0] = &modemserialbtn;
#if (0)
	buttons[1] = &internetbtn;
#endif	//(0)
	if (ipx_avail){
		buttons[1] = &ipxbtn;
		buttons[2] = &cancelbtn;
	}else{
		buttons[1] = &cancelbtn;
		number_of_buttons--;
	}

	buttons[curbutton]->Turn_On();

	Keyboard::Clear();

	Fancy_Text_Print(TXT_NONE, 0, 0, CC_GREEN, TBLACK,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW);

	/*
	-------------------------- Main Processing Loop --------------------------
	*/
	display = REDRAW_ALL;
	process = true;
	pressed = false;
	while (process) {
		/*
		** If we have just received input focus again after running in the background then
		** we need to redraw.
		*/
		if (AllSurfaces.SurfacesRestored){
			AllSurfaces.SurfacesRestored=FALSE;
			display=REDRAW_ALL;
		}

		/*
		........................ Invoke game callback .........................
		*/
		Call_Back();
		/*
		...................... Refresh display if needed ......................
		*/
		if (display) {
			Hide_Mouse();
			if (display >= REDRAW_BACKGROUND) {
				/*
				..................... Refresh the backdrop ......................
				*/
				Load_Title_Screen("HTITLE.PCX", &HidPage, Palette);
				HidPage.Blit(SeenBuff);
				/*
				..................... Draw the background .......................
				*/
				Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);
				Draw_Caption (TXT_SELECT_MPLAYER_GAME, d_dialog_x, d_dialog_y, d_dialog_w);
			}
			/*
			.......................... Redraw buttons ..........................
			*/
			if (display >= REDRAW_BUTTONS) {
				commands->Flag_List_To_Redraw();
			}
			Show_Mouse();
			display = REDRAW_NONE;
		}

		/*
		........................... Get user input ............................
		*/
		input = commands->Input();

		/*
		............................ Process input ............................
		*/
		switch (input) {
			case (BUTTON_MODEMSERIAL | KN_BUTTON):
				selection = BUTTON_MODEMSERIAL;
				pressed = true;
				break;

#if	(0)
			case (BUTTON_INTERNET | KN_BUTTON):
				selection = BUTTON_INTERNET;
				pressed = true;
				break;
#endif	//(0)

			case (BUTTON_IPX | KN_BUTTON):
				selection = BUTTON_IPX;
				pressed = true;
				break;

			case (KN_ESC):
			case (BUTTON_CANCEL | KN_BUTTON):
				selection = BUTTON_CANCEL;
				pressed = true;
				break;

			case KN_UP:
				buttons[curbutton]->Turn_Off();
				buttons[curbutton]->Flag_To_Redraw();
				curbutton--;
				if (curbutton < 0)
					curbutton = (number_of_buttons - 1);
				buttons[curbutton]->Turn_On();
				buttons[curbutton]->Flag_To_Redraw();
				break;

			case KN_DOWN:
				buttons[curbutton]->Turn_Off();
				buttons[curbutton]->Flag_To_Redraw();
				curbutton++;
				if (curbutton > (number_of_buttons - 1) )
					curbutton = 0;
				buttons[curbutton]->Turn_On();
				buttons[curbutton]->Flag_To_Redraw();
				break;

			case KN_RETURN:
				selection = curbutton + BUTTON_MODEMSERIAL;
				if (!ipx_avail) selection--;
				pressed = true;
				break;

			default:
				break;
		}

		if (pressed) {
			//
			// to make sure the selection is correct in case they used the mouse
			//
			buttons[curbutton]->Turn_Off();
			buttons[curbutton]->Flag_To_Redraw();
			curbutton = selection - BUTTON_MODEMSERIAL;
			if (!ipx_avail && curbutton >= number_of_buttons)
				curbutton = number_of_buttons - 1;
			buttons[curbutton]->Turn_On();
//			buttons[curbutton]->Flag_To_Redraw();
			buttons[curbutton]->IsPressed = true;
			buttons[curbutton]->Draw_Me(true);

			switch (selection) {
				case (BUTTON_MODEMSERIAL):

					//
					// Pop up the modem/serial/com port dialog
					//
					retval = Com_Scenario_Dialog() ? GAME_SKIRMISH : GAME_NORMAL;

					if (retval != GAME_NORMAL) {
						process = false;
					} else {
						buttons[curbutton]->IsPressed = false;
						display = REDRAW_ALL;
					}
					break;

#if	(0)
				case (BUTTON_INTERNET):
//#define ONLY_FOR_E3
#ifdef ONLY_FOR_E3
					Call_Back();
					Show_Internet_Connection_Progress();		//changed to do nothing
					Hide_Mouse();
					Load_Title_Screen("HTITLE.PCX", &HidPage, Palette);
					HidPage.Blit(SeenBuff);
					Show_Mouse();
					Call_Back();
					CCMessageBox().Process("Error! - Unable to ping KANE.WESTWOOD.COM");

					buttons[curbutton]->IsPressed = false;
					display = REDRAW_ALL;


#endif	//ONLY_FOR_E3


#ifdef FORCE_WINSOCK
					if (Winsock.Init()){
						Read_MultiPlayer_Settings ();
						int result = Choose_Internet_Game();

						if (!result){
							Winsock.Close();
							buttons[curbutton]->IsPressed = false;
							display = REDRAW_ALL;
							break;
						}

						result = Get_Internet_Host_Or_Join();
						if (result == 1){
							Winsock.Close();
							buttons[curbutton]->IsPressed = false;
							display = REDRAW_ALL;
							break;
						}
						Server = !result;

						if (Server){
#if (0)
							ModemGameToPlay = INTERNET_HOST;
							Winsock.Start_Server();
#else
							result = Get_IP_Address();
							if (!result){
								Winsock.Close();
								buttons[curbutton]->IsPressed = false;
								display = REDRAW_ALL;
								break;
							}
							Winsock.Set_Host_Address(PlanetWestwoodIPAddress);
							Winsock.Start_Server();
#endif

						}else{
							ModemGameToPlay = INTERNET_JOIN;
							result = Get_IP_Address();
							if (!result){
								Winsock.Close();
								buttons[curbutton]->IsPressed = false;
								display = REDRAW_ALL;
								break;
							}
							Winsock.Set_Host_Address(PlanetWestwoodIPAddress);
							Winsock.Start_Client();
						}

						//CountDownTimerClass connect_timeout;
						//connect_timeout.Set(15*60);

						////Show_Internet_Connection_Progress();

						if (!Winsock.Get_Connected()){
							Winsock.Close();
							return(GAME_NORMAL);
						}

						SerialSettingsType *settings;
						settings = &SerialDefaults;
						Init_Null_Modem(settings);
						if (Server){
							if (Com_Scenario_Dialog()){
								return (GAME_INTERNET);
							}else{
								Winsock.Close();
								return (GAME_NORMAL);
							}
						}else{
							if (Com_Show_Scenario_Dialog()){
								return (GAME_INTERNET);
							}else{
								Winsock.Close();
								return (GAME_NORMAL);
							}
						}
					}
#endif //FORCE_WINSOCK
					break;

#endif	//(0)

				case (BUTTON_IPX):
					retval = GAME_IPX;
					process = false;
					break;

				case (BUTTON_CANCEL):
					retval = GAME_NORMAL;
					process = false;
					break;
			}

			pressed = false;
		}
	}
	return(retval);
}


/***********************************************************************************************
 * Com_Scenario_Dialog -- Local skirmish scenario selection dialog.                            *
 *                                                                                             *
 *    This dialog reuses TD's existing multiplayer globals and map discovery so the engine     *
 *    can start a local skirmish without entering the legacy transport setup paths.            *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      bool; Was the skirmish accepted by the user?                                           *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   03/26/2026 EA : Created.                                                                  *
 *=============================================================================================*/
int Com_Scenario_Dialog(void)
{
	int factor			= (SeenBuff.Get_Width() == 320) ? 1 : 2;
	int d_dialog_w = 290*factor;
	int d_dialog_h = 178*factor;
	int d_dialog_x = ((SeenBuff.Get_Width() - d_dialog_w) / 2);
	int d_dialog_y = ((SeenBuff.Get_Height() - d_dialog_h) / 2);
	int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);

	int d_txt6_h = 6*factor+1;
	int d_margin1 = 5*factor;
	int d_margin2 = 2*factor;

	int d_name_w = 70*factor;
	int d_name_h = 9*factor;
	int d_name_x = d_dialog_x + 108*factor;
	int d_name_y = d_dialog_y + d_margin1 + d_txt6_h + d_txt6_h + d_margin1;

	int d_credits_w = ((CREDITSBUF_MAX - 1) * 6*factor) + 3*factor;
	int d_credits_h = 9*factor;
	int d_credits_x = d_name_x;
	int d_credits_y = d_name_y + d_name_h + d_margin2;

	int d_gdi_w = 30*factor;
	int d_gdi_h = 9*factor;
	int d_gdi_x = d_dialog_cx + (d_dialog_w / 4);
	int d_gdi_y = d_dialog_y + d_margin1 + d_txt6_h + d_txt6_h + d_margin1;

	int d_nod_w = 30*factor;
	int d_nod_h = 9*factor;
	int d_nod_x = d_gdi_x + d_gdi_w + (d_margin1 / 2);
	int d_nod_y = d_gdi_y;

	int d_scenariolist_w = 182*factor;
	int d_scenariolist_h = 54*factor;
	int d_scenariolist_x = d_dialog_cx - (d_scenariolist_w / 2);
	int d_scenariolist_y = d_credits_y + d_credits_h + d_margin1 + d_txt6_h;

	int d_count_w = 25*factor;
	int d_count_h = 7*factor;
	int d_count_y = d_scenariolist_y + d_scenariolist_h + factor;

	int d_level_w = 25*factor;
	int d_level_h = 7*factor;
	int d_level_y = d_count_y;

	int d_bases_w = 110*factor;
	int d_bases_h = 9*factor;
	int d_bases_x = d_dialog_cx - d_bases_w - d_margin2;
	int d_bases_y = d_count_y + d_count_h + factor;

	int d_goodies_w = 110*factor;
	int d_goodies_h = 9*factor;
	int d_goodies_x = d_dialog_cx + d_margin2;
	int d_goodies_y = d_bases_y;

	int d_count_x = d_dialog_cx - d_count_w - ((2 * 6*factor) + 3*factor)
					- ((d_bases_w - ((13 * 6*factor) + 3*factor + d_count_w)) / 2) - d_margin2;

	int d_level_x = d_dialog_cx + (11 * 6*factor)
					+ ((d_goodies_w - ((13 * 6*factor) + 3*factor + d_level_w)) / 2) + d_margin2;

	int d_tiberium_w = 110*factor;
	int d_tiberium_h = 9*factor;
	int d_tiberium_x = d_dialog_cx - d_bases_w - d_margin2;
	int d_tiberium_y = d_dialog_y + d_dialog_h - d_margin1 - d_tiberium_h - 9*factor;

	int d_ghosts_w = 110*factor;
	int d_ghosts_h = 9*factor;
	int d_ghosts_x = d_dialog_cx + d_margin2;
	int d_ghosts_y = d_tiberium_y;

	int d_ok_w = 45*factor;
	int d_ok_h = 9*factor;
	int d_ok_x = d_tiberium_x + (d_tiberium_w / 2) - (d_ok_w / 2);
	int d_ok_y = d_tiberium_y + d_tiberium_h + d_margin1;

	int d_cancel_w = 45*factor;
	int d_cancel_h = 9*factor;
	int d_cancel_x = d_ghosts_x + (d_ghosts_w / 2) - (d_cancel_w / 2);
	int d_cancel_y = d_tiberium_y + d_tiberium_h + d_margin1;

	enum {
		BUTTON_NAME = 100,
		BUTTON_GDI,
		BUTTON_NOD,
		BUTTON_CREDITS,
		BUTTON_SCENARIOLIST,
		BUTTON_COUNT,
		BUTTON_LEVEL,
		BUTTON_BASES,
		BUTTON_TIBERIUM,
		BUTTON_GOODIES,
		BUTTON_GHOSTS,
		BUTTON_OK,
		BUTTON_CANCEL,
	};

	typedef enum {
		REDRAW_NONE = 0,
		REDRAW_BUTTONS,
		REDRAW_BACKGROUND,
		REDRAW_ALL = REDRAW_BACKGROUND
	} RedrawType;

	RedrawType display = REDRAW_ALL;
	bool process = true;
	KeyNumType input;
	int retcode = 0;
	char namebuf[MPLAYER_NAME_MAX] = {0};
	char credbuf[CREDITSBUF_MAX];
	void const *up_button;
	void const *down_button;
	static int first_time = 1;

	if (MPlayerScenarios.Count() == 0) {
		Read_Scenario_Descriptions();
	}
	if (MPlayerScenarios.Count() == 0 || MPlayerFilenum.Count() == 0) {
		return(0);
	}

	if (InMainLoop){
		up_button = Hires_Retrieve("BTN-UP.SHP");
		down_button = Hires_Retrieve("BTN-DN.SHP");
	}else{
		up_button = Hires_Retrieve("BTN-UP2.SHP");
		down_button = Hires_Retrieve("BTN-DN2.SHP");
	}

	EditClass name_edt (BUTTON_NAME,
		namebuf, MPLAYER_NAME_MAX,
		TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_name_x, d_name_y, d_name_w, d_name_h, EditClass::ALPHANUMERIC);

	TextButtonClass gdibtn(BUTTON_GDI, TXT_G_D_I,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_gdi_x, d_gdi_y, d_gdi_w, d_gdi_h);

	TextButtonClass nodbtn(BUTTON_NOD, TXT_N_O_D,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_nod_x, d_nod_y, d_nod_w, d_nod_h);

	EditClass credit_edt (BUTTON_CREDITS,
		credbuf, CREDITSBUF_MAX,
		TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_credits_x, d_credits_y, d_credits_w, d_credits_h, EditClass::ALPHANUMERIC);

	ListClass scenariolist(BUTTON_SCENARIOLIST,
		d_scenariolist_x, d_scenariolist_y, d_scenariolist_w, d_scenariolist_h,
		TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		up_button,
		down_button);

	GaugeClass countgauge (BUTTON_COUNT,
		d_count_x, d_count_y, d_count_w, d_count_h);

	GaugeClass levelgauge (BUTTON_LEVEL,
		d_level_x, d_level_y, d_level_w, d_level_h);

	TextButtonClass basesbtn(BUTTON_BASES, TXT_BASES_OFF,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_bases_x, d_bases_y, d_bases_w, d_bases_h);

	TextButtonClass tiberiumbtn(BUTTON_TIBERIUM, TXT_TIBERIUM_OFF,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_tiberium_x, d_tiberium_y, d_tiberium_w, d_tiberium_h);

	TextButtonClass goodiesbtn(BUTTON_GOODIES, TXT_CRATES_OFF,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_goodies_x, d_goodies_y, d_goodies_w, d_goodies_h);

	TextButtonClass ghostsbtn(BUTTON_GHOSTS, TXT_AI_PLAYERS_OFF,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_ghosts_x, d_ghosts_y, d_ghosts_w, d_ghosts_h);

	TextButtonClass okbtn(BUTTON_OK, TXT_OK,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_ok_x, d_ok_y, d_ok_w, d_ok_h);

	TextButtonClass cancelbtn(BUTTON_CANCEL, TXT_CANCEL,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_cancel_x, d_cancel_y, d_cancel_w, d_cancel_h);

	GadgetClass *commands;

	commands = &name_edt;
	gdibtn.Add_Tail(*commands);
	nodbtn.Add_Tail(*commands);
	credit_edt.Add_Tail(*commands);
	scenariolist.Add_Tail(*commands);
	countgauge.Add_Tail(*commands);
	levelgauge.Add_Tail(*commands);
	basesbtn.Add_Tail(*commands);
	tiberiumbtn.Add_Tail(*commands);
	goodiesbtn.Add_Tail(*commands);
	ghostsbtn.Add_Tail(*commands);
	okbtn.Add_Tail(*commands);
	cancelbtn.Add_Tail(*commands);

	MPlayerColorIdx = MPlayerPrefColor;
	strcpy(namebuf, MPlayerName);
	name_edt.Set_Text(namebuf, MPLAYER_NAME_MAX);
	name_edt.Set_Color(MPlayerTColors[MPlayerColorIdx]);

	if (MPlayerHouse == HOUSE_GOOD) {
		gdibtn.Turn_On();
	} else {
		nodbtn.Turn_On();
	}

	if (first_time) {
		MPlayerCredits = 3000;
		MPlayerBases = 1;
		MPlayerTiberium = 0;
		MPlayerGoodies = 0;
		MPlayerGhosts = 1;
		MPlayerUnitCount = (MPlayerCountMax[MPlayerBases] + MPlayerCountMin[MPlayerBases]) / 2;
		first_time = 0;
	}

	if (MPlayerBases) {
		basesbtn.Turn_On();
		basesbtn.Set_Text(TXT_BASES_ON);
	}
	if (MPlayerTiberium) {
		tiberiumbtn.Turn_On();
		tiberiumbtn.Set_Text(TXT_TIBERIUM_ON);
	}
	if (MPlayerGoodies) {
		goodiesbtn.Turn_On();
		goodiesbtn.Set_Text(TXT_CRATES_ON);
	}
	if (MPlayerGhosts) {
		ghostsbtn.Turn_On();
		ghostsbtn.Set_Text(TXT_AI_PLAYERS_ON);
	}

	sprintf(credbuf, "%d", MPlayerCredits);
	credit_edt.Set_Text(credbuf, CREDITSBUF_MAX);
	levelgauge.Set_Maximum(MPLAYER_BUILD_LEVEL_MAX - 1);
	levelgauge.Set_Value(BuildLevel - 1);
	countgauge.Set_Maximum(MPlayerCountMax[MPlayerBases] - MPlayerCountMin[MPlayerBases]);
	countgauge.Set_Value(MPlayerUnitCount - MPlayerCountMin[MPlayerBases]);
	Special.IsTGrowth = MPlayerTiberium;
	Special.IsTSpread = MPlayerTiberium;

	for (int i = 0; i < MPlayerScenarios.Count(); i++) {
		scenariolist.Add_Item(strupr(MPlayerScenarios[i]));
	}
	if (ScenarioIdx < 0 || ScenarioIdx >= MPlayerScenarios.Count()) {
		ScenarioIdx = 0;
	}
	scenariolist.Set_Selected_Index(ScenarioIdx);

	Set_Logic_Page(SeenBuff);
	Keyboard::Clear();

	while (process) {
		if (AllSurfaces.SurfacesRestored){
			AllSurfaces.SurfacesRestored=FALSE;
			display=REDRAW_ALL;
		}

		Call_Back();

		if (display) {
			Hide_Mouse();
			Load_Title_Screen("HTITLE.PCX", &HidPage, Palette);
			HidPage.Blit(SeenBuff);
			Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);

			Fancy_Text_Print(TXT_NONE, 0, 0, CC_GREEN, TBLACK,
				TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW);
			Draw_Caption(TXT_NONE, d_dialog_x, d_dialog_y, d_dialog_w);
			Fancy_Text_Print("Skirmish",
				d_dialog_cx, d_dialog_y + d_margin1 + 1*factor,
				CC_GREEN, TBLACK,
				TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW);

			Fancy_Text_Print(TXT_YOUR_NAME,
				d_name_x - 5*factor, d_name_y + 1*factor,
				CC_GREEN, TBLACK,
				TPF_RIGHT | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW);

			Fancy_Text_Print(TXT_SIDE_COLON,
				d_gdi_x - 5*factor, d_gdi_y + 1*factor,
				CC_GREEN, TBLACK,
				TPF_RIGHT | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW);

			Fancy_Text_Print(TXT_START_CREDITS_COLON,
				d_credits_x - 5*factor, d_credits_y + 1*factor,
				CC_GREEN, TBLACK,
				TPF_RIGHT | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW);

			Fancy_Text_Print(TXT_SCENARIOS,
				d_scenariolist_x + (d_scenariolist_w / 2),
				d_scenariolist_y - d_txt6_h,
				CC_GREEN, TBLACK,
				TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW);

			Fancy_Text_Print(TXT_COUNT, d_count_x - 3*factor, d_count_y, CC_GREEN, TBLACK,
				TPF_NOSHADOW | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_RIGHT);

			Fancy_Text_Print(TXT_LEVEL, d_level_x - 3*factor, d_level_y, CC_GREEN, TBLACK,
				TPF_NOSHADOW | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_RIGHT);

			commands->Flag_List_To_Redraw();
			Show_Mouse();
			display = REDRAW_NONE;
		}

		input = commands->Input();

		switch (input) {
			case (BUTTON_NAME | KN_BUTTON):
				credit_edt.Clear_Focus();
				credit_edt.Flag_To_Redraw();
				strcpy(MPlayerName, namebuf);
				break;

			case (BUTTON_GDI | KN_BUTTON):
				MPlayerHouse = HOUSE_GOOD;
				gdibtn.Turn_On();
				nodbtn.Turn_Off();
				break;

			case (BUTTON_NOD | KN_BUTTON):
				MPlayerHouse = HOUSE_BAD;
				gdibtn.Turn_Off();
				nodbtn.Turn_On();
				break;

			case (BUTTON_CREDITS | KN_BUTTON):
				name_edt.Clear_Focus();
				name_edt.Flag_To_Redraw();
				MPlayerCredits = atoi(credbuf);
				break;

			case (BUTTON_SCENARIOLIST | KN_BUTTON):
				if (scenariolist.Current_Index() != ScenarioIdx) {
					ScenarioIdx = scenariolist.Current_Index();
				}
				break;

			case (BUTTON_COUNT | KN_BUTTON):
				MPlayerUnitCount = countgauge.Get_Value() + MPlayerCountMin[MPlayerBases];
				break;

			case (BUTTON_LEVEL | KN_BUTTON):
				BuildLevel = levelgauge.Get_Value() + 1;
				if (BuildLevel > MPLAYER_BUILD_LEVEL_MAX)
					BuildLevel = MPLAYER_BUILD_LEVEL_MAX;
				break;

			case (BUTTON_BASES | KN_BUTTON):
				if (MPlayerBases) {
					MPlayerBases = 0;
					basesbtn.Turn_Off();
					basesbtn.Set_Text(TXT_BASES_OFF);
					MPlayerUnitCount = Fixed_To_Cardinal (MPlayerCountMax[0]-MPlayerCountMin[0],
						Cardinal_To_Fixed(MPlayerCountMax[1]-MPlayerCountMin[1],
						MPlayerUnitCount-MPlayerCountMin[1])) + MPlayerCountMin[0];
				} else {
					MPlayerBases = 1;
					basesbtn.Turn_On();
					basesbtn.Set_Text(TXT_BASES_ON);
					MPlayerUnitCount = Fixed_To_Cardinal (MPlayerCountMax[1]-MPlayerCountMin[1],
						Cardinal_To_Fixed(MPlayerCountMax[0]-MPlayerCountMin[0],
						MPlayerUnitCount-MPlayerCountMin[0])) + MPlayerCountMin[1];
				}
				countgauge.Set_Maximum(MPlayerCountMax[MPlayerBases] - MPlayerCountMin[MPlayerBases]);
				countgauge.Set_Value(MPlayerUnitCount - MPlayerCountMin[MPlayerBases]);
				display = REDRAW_ALL;
				break;

			case (BUTTON_TIBERIUM | KN_BUTTON):
				if (MPlayerTiberium) {
					MPlayerTiberium = 0;
					Special.IsTGrowth = 0;
					Special.IsTSpread = 0;
					tiberiumbtn.Turn_Off();
					tiberiumbtn.Set_Text(TXT_TIBERIUM_OFF);
				} else {
					MPlayerTiberium = 1;
					Special.IsTGrowth = 1;
					Special.IsTSpread = 1;
					tiberiumbtn.Turn_On();
					tiberiumbtn.Set_Text(TXT_TIBERIUM_ON);
				}
				break;

			case (BUTTON_GOODIES | KN_BUTTON):
				if (MPlayerGoodies) {
					MPlayerGoodies = 0;
					goodiesbtn.Turn_Off();
					goodiesbtn.Set_Text(TXT_CRATES_OFF);
				} else {
					MPlayerGoodies = 1;
					goodiesbtn.Turn_On();
					goodiesbtn.Set_Text(TXT_CRATES_ON);
				}
				break;

			case (BUTTON_GHOSTS | KN_BUTTON):
				if (MPlayerGhosts) {
					MPlayerGhosts = 0;
					ghostsbtn.Turn_Off();
					ghostsbtn.Set_Text(TXT_AI_PLAYERS_OFF);
				} else {
					MPlayerGhosts = 1;
					ghostsbtn.Turn_On();
					ghostsbtn.Set_Text(TXT_AI_PLAYERS_ON);
				}
				break;

			case (KN_RETURN):
			case (BUTTON_OK | KN_BUTTON):
				MPlayerCredits = atoi(credbuf);
				strcpy(MPlayerName, namebuf);
				Prepare_Skirmish_Player_State(namebuf, scenariolist.Current_Index());
				retcode = 1;
				process = false;
				break;

			case (KN_ESC):
			case (BUTTON_CANCEL | KN_BUTTON):
				retcode = 0;
				process = false;
				break;

			default:
				break;
		}
	}

	return (retcode);
}


static void Prepare_Skirmish_Player_State(char const *namebuf, int scenario_index)
{
	char const *player_name = namebuf;

	if (player_name == NULL || *player_name == '\0') {
		player_name = "Player";
	}

	if (scenario_index < 0 || scenario_index >= MPlayerFilenum.Count()) {
		scenario_index = 0;
	}

	ScenarioIdx = scenario_index;
	Scenario = MPlayerFilenum[ScenarioIdx];
	MPlayerCount = 1;
	MPlayerLocalID = Build_MPlayerID(MPlayerPrefColor, MPlayerHouse);
	MPlayerID[0] = MPlayerLocalID;
	MPlayerHouses[0] = HOUSE_MULTI1;
	memset(MPlayerNames[0], 0, sizeof(MPlayerNames[0]));
	strncpy(MPlayerNames[0], player_name, MPLAYER_NAME_MAX - 1);
	memset(MPlayerName, 0, sizeof(MPlayerName));
	strncpy(MPlayerName, player_name, MPLAYER_NAME_MAX - 1);

	for (int i = 1; i < MAX_PLAYERS; i++) {
		MPlayerID[i] = 0;
		MPlayerHouses[i] = HOUSE_NONE;
		memset(MPlayerNames[i], 0, sizeof(MPlayerNames[i]));
	}

	if (BuildLevel < 1) {
		BuildLevel = 1;
	}
	if (BuildLevel > MPLAYER_BUILD_LEVEL_MAX) {
		BuildLevel = MPLAYER_BUILD_LEVEL_MAX;
	}
}


/***********************************************************************************************
 * Read_MultiPlayer_Settings -- reads multi-player settings from conquer.ini                   *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
void Read_MultiPlayer_Settings (void)
{
	char *buffer;							// INI staging buffer pointer.
	char *tbuffer;							// Accumulation buffer of trigger IDs.
	int len;									// Length of data in buffer.
	char *tokenptr;						// ptr to token
	PhoneEntryClass *phone;				// a phone book entry
	char *entry;							// a phone book entry
	char buf[128];							// buffer for parsing INI entry
	int i;
	CELL cell;

	/*------------------------------------------------------------------------
	Fetch working pointer to the INI staging buffer. Make sure that the buffer
	is cleared out before proceeding.  (Don't use the HidPage for this, since
	the HidPage may be needed for various uncompressions during the INI
	parsing.)
	------------------------------------------------------------------------*/
	buffer = (char *)_ShapeBuffer;
	memset(buffer, '\0', _ShapeBufferSize);

	/*------------------------------------------------------------------------
	Clear the initstring entries
	------------------------------------------------------------------------*/
	for (i = 0; i < InitStrings.Count(); i++) {
		delete[] InitStrings[i];
	}
	InitStrings.Clear();

	/*------------------------------------------------------------------------
	Clear the dialing entries
	------------------------------------------------------------------------*/
	for (i = 0; i < PhoneBook.Count(); i++) {
		delete PhoneBook[i];
	}
	PhoneBook.Clear();

	/*------------------------------------------------------------------------
	Create filename and read the file.
	------------------------------------------------------------------------*/
	CCFileClass file ("CONQUER.INI");
	if (!file.Is_Available()) {
		return;
	} else {
		file.Read(buffer, _ShapeBufferSize-1);
	}
	file.Close();

	if (!Special.IsFromWChat){
		/*------------------------------------------------------------------------
		Get the player's last-used Handle
		------------------------------------------------------------------------*/
		WWGetPrivateProfileString("MultiPlayer", "Handle", "Noname", MPlayerName,
			sizeof(MPlayerName), buffer);

		/*------------------------------------------------------------------------
		Get the player's last-used Color
		------------------------------------------------------------------------*/
		MPlayerPrefColor = WWGetPrivateProfileInt("MultiPlayer", "Color", 0, buffer);
		MPlayerHouse = (HousesType)WWGetPrivateProfileInt("MultiPlayer", "Side",
			HOUSE_GOOD, buffer);
		CurPhoneIdx = WWGetPrivateProfileInt("MultiPlayer", "PhoneIndex", -1, buffer);
	}else{
		CurPhoneIdx = -1;
	}

#if 1
	TrapCheckHeap = WWGetPrivateProfileInt( "MultiPlayer", "CheckHeap", 0, buffer);
#endif

	/*------------------------------------------------------------------------
	Read in default serial settings
	------------------------------------------------------------------------*/
	WWGetPrivateProfileString ("SerialDefaults", "ModemName", "NoName", SerialDefaults.ModemName, MODEM_NAME_MAX, buffer);
	if (!strcmp ( SerialDefaults.ModemName, "NoName")) {
		SerialDefaults.ModemName[0] = 0;
	}
	WWGetPrivateProfileString ("SerialDefaults", "Port", "0", buf, 5, buffer);
	sscanf (buf, "%x", &SerialDefaults.Port);
	SerialDefaults.IRQ = WWGetPrivateProfileInt("SerialDefaults", "IRQ", -1, buffer);
	SerialDefaults.Baud = WWGetPrivateProfileInt("SerialDefaults", "Baud", -1, buffer);
	SerialDefaults.Init = WWGetPrivateProfileInt("SerialDefaults", "Init", 0, buffer);
	SerialDefaults.Compression = WWGetPrivateProfileInt ("SerialDefaults", "Compression", 0, buffer);
	SerialDefaults.ErrorCorrection = WWGetPrivateProfileInt ("SerialDefaults", "ErrorCorrection", 0, buffer);
	SerialDefaults.HardwareFlowControl = WWGetPrivateProfileInt ("SerialDefaults", "HardwareFlowControl", 1, buffer);
	WWGetPrivateProfileString ("SerialDefaults", "DialMethod", "T",
		buf, 2, buffer);


	// find dial method

	for (i = 0; i < DIAL_METHODS; i++) {
		if ( !strcmpi( buf, DialMethodCheck[ i ]) ) {
			SerialDefaults.DialMethod = (DialMethodType)i;
			break;
		}
	}

	// if method not found set to touch tone

	if (i == DIAL_METHODS) {
		SerialDefaults.DialMethod = DIAL_TOUCH_TONE;
	}

	SerialDefaults.InitStringIndex =
											WWGetPrivateProfileInt("SerialDefaults",
												"InitStringIndex", 0, buffer);

	SerialDefaults.CallWaitStringIndex =
											WWGetPrivateProfileInt("SerialDefaults",
												"CallWaitStringIndex", CALL_WAIT_CUSTOM,
												buffer);

	WWGetPrivateProfileString ("SerialDefaults", "CallWaitString", "",
		SerialDefaults.CallWaitString, CWAITSTRBUF_MAX, buffer);

	if (SerialDefaults.IRQ == 0 ||
		SerialDefaults.Baud == 0) {

		SerialDefaults.Port = 0;
		SerialDefaults.IRQ = -1;
		SerialDefaults.Baud = -1;
	}

	/*------------------------------------------------------------------------
	Set 'tbuffer' to point past the actual INI data
	------------------------------------------------------------------------*/
	len = strlen(buffer) + 2;
	tbuffer = buffer + len;

	/*------------------------------------------------------------------------
	Read all Base-Scenario names into 'tbuffer'
	------------------------------------------------------------------------*/
	WWGetPrivateProfileString("InitStrings", NULL, NULL, tbuffer,
		ShapeBufferSize-len, buffer);

	/*------------------------------------------------------------------------
	Read in & store each entry
	------------------------------------------------------------------------*/
	while (*tbuffer != '\0') {
		entry = new char[ INITSTRBUF_MAX ];

		entry[0] = 0;

		WWGetPrivateProfileString("InitStrings", tbuffer, NULL, entry,
			INITSTRBUF_MAX, buffer);

		strupr( entry );

		InitStrings.Add( entry );

		tbuffer += strlen(tbuffer) + 1;
	}

	// if no entries then have at least one

	if ( tbuffer == (buffer + len) ) {
		entry = new char[ INITSTRBUF_MAX ];
		strcpy( entry, "ATZ" );
		InitStrings.Add( entry );
		SerialDefaults.InitStringIndex = 0;
	} else {
		len = strlen(buffer) + 2;
	}

	/*------------------------------------------------------------------------
	Repeat the process for the phonebook
	------------------------------------------------------------------------*/
	tbuffer = buffer + len;

	/*------------------------------------------------------------------------
	Read in all phone book listings.
	Format: Name=PhoneNum,Port,IRQ,Baud,InitString
	------------------------------------------------------------------------*/

	/*........................................................................
	Read the entry names in
	........................................................................*/
	WWGetPrivateProfileString("PhoneBook", NULL, NULL, tbuffer,
		ShapeBufferSize-len, buffer);

	while (*tbuffer != '\0') {
		/*.....................................................................
		Create a new phone book entry
		.....................................................................*/
		phone = new PhoneEntryClass();

		/*.....................................................................
		Read the entire entry in
		.....................................................................*/
		WWGetPrivateProfileString("PhoneBook", tbuffer, NULL, buf, 128, buffer);

		/*.....................................................................
		Extract name, phone # & serial port settings
		.....................................................................*/
		tokenptr = strtok( buf, "|" );
		if (tokenptr) {
			strcpy( phone->Name, tokenptr );
			strupr( phone->Name );
		} else {
			phone->Name[0] = 0;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			strcpy( phone->Number, tokenptr );
			strupr( phone->Number );
		} else {
			phone->Number[0] = 0;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			sscanf( tokenptr, "%x", &phone->Settings.Port );
		} else {
			phone->Settings.Port = 0;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			phone->Settings.IRQ = atoi( tokenptr );
		} else {
			phone->Settings.IRQ = -1;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			phone->Settings.Baud = atoi( tokenptr );
		} else {
			phone->Settings.Baud = -1;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			phone->Settings.Compression = atoi( tokenptr );
		} else {
			phone->Settings.Compression = 0;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			phone->Settings.ErrorCorrection = atoi( tokenptr );
		} else {
			phone->Settings.ErrorCorrection = 0;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			phone->Settings.HardwareFlowControl = atoi( tokenptr );
		} else {
			phone->Settings.HardwareFlowControl = 1;
		}


		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			strcpy( buf, tokenptr );

			// find dial method

			for (i = 0; i < DIAL_METHODS; i++) {
				if ( !strcmpi( buf, DialMethodCheck[ i ]) ) {
					phone->Settings.DialMethod = (DialMethodType)i;
					break;
				}
			}

			// if method not found set to touch tone

			if (i == DIAL_METHODS) {
				phone->Settings.DialMethod = DIAL_TOUCH_TONE;
			}
		} else {
			phone->Settings.DialMethod = DIAL_TOUCH_TONE;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			phone->Settings.InitStringIndex = atoi( tokenptr );
		} else {
			phone->Settings.InitStringIndex = 0;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			phone->Settings.CallWaitStringIndex = atoi( tokenptr );
		} else {
			phone->Settings.CallWaitStringIndex = CALL_WAIT_CUSTOM;
		}

		tokenptr = strtok( NULL, "|" );
		if (tokenptr) {
			strcpy (phone->Settings.CallWaitString, tokenptr);
		} else {
			phone->Settings.CallWaitString[0] = 0;
		}

		/*.....................................................................
		Add it to our list
		.....................................................................*/
		PhoneBook.Add(phone);

		tbuffer += strlen(tbuffer) + 1;
	}

	/*------------------------------------------------------------------------
	Read special recording playback values, to help find sync bugs
	------------------------------------------------------------------------*/
	if (PlaybackGame) {
		TrapFrame = WWGetPrivateProfileInt ("SyncBug","Frame",0x7fffffff, buffer);

		TrapObjType = (RTTIType)WWGetPrivateProfileInt ("SyncBug","Type",RTTI_NONE, buffer);
		WWGetPrivateProfileString ("SyncBug","Type","NONE",buf,80,buffer);
		if (!stricmp(buf,"AIRCRAFT"))
			TrapObjType = RTTI_AIRCRAFT;
		else if (!stricmp(buf,"ANIM"))
			TrapObjType = RTTI_ANIM;
		else if (!stricmp(buf,"BUILDING"))
			TrapObjType = RTTI_BUILDING;
		else if (!stricmp(buf,"BULLET"))
			TrapObjType = RTTI_BULLET;
		else if (!stricmp(buf,"INFANTRY"))
			TrapObjType = RTTI_INFANTRY;
		else if (!stricmp(buf,"UNIT"))
			TrapObjType = RTTI_UNIT;
		else {
			TrapObjType = RTTI_NONE;
		}

		WWGetPrivateProfileString ("SyncBug","Coord","0",buf,80,buffer);
		{ unsigned int tmp; sscanf(buf,"%x",&tmp); TrapCoord = (COORDINATE)tmp; } /* LP64: was %lx into uint32_t */

		WWGetPrivateProfileString ("SyncBug","this","0",buf,80,buffer);
		{ uintptr_t tmp; sscanf(buf,"%lx",(unsigned long*)&tmp); TrapThis = (void*)tmp; }

		WWGetPrivateProfileString ("SyncBug","Cell","0",buf,80,buffer);
		cell = atoi(buf);
		if (cell) {
			TrapCell = &(Map[cell]);
		}
	}
}


/***********************************************************************************************
 * Write_MultiPlayer_Settings -- writes multi-player settings to conquer.ini                   *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
void Write_MultiPlayer_Settings (void)
{
	char * buffer;			// INI staging buffer pointer.
	CCFileClass file;
	int i;
	char entrytext[16];
	char buf[512];							// buffer for parsing INI entry

	/*------------------------------------------------------------------------
	Get a working pointer to the INI staging buffer. Make sure that the buffer
	starts cleared out of any data.
	------------------------------------------------------------------------*/
	buffer = (char *)_ShapeBuffer;
	memset(buffer, '\0', _ShapeBufferSize);

	file.Set_Name("CONQUER.INI");
	if (file.Is_Available()) {
		file.Open(READ);
		file.Read(buffer, _ShapeBufferSize-1);
		file.Close();
	}

	/*------------------------------------------------------------------------
	Save the player's last-used Handle & Color
	------------------------------------------------------------------------*/
	WWWritePrivateProfileInt("MultiPlayer", "PhoneIndex", CurPhoneIdx, buffer);
	WWWritePrivateProfileInt ("MultiPlayer", "Color", MPlayerPrefColor, buffer);
	WWWritePrivateProfileInt ("MultiPlayer", "Side", MPlayerHouse, buffer);
	WWWritePrivateProfileString("MultiPlayer", "Handle", MPlayerName, buffer);

	/*------------------------------------------------------------------------
	Clear all existing SerialDefault entries.
	------------------------------------------------------------------------*/
	WWWritePrivateProfileString ("SerialDefaults", NULL, NULL, buffer);

	/*------------------------------------------------------------------------
	Save default serial settings in opposite order you want to see them
	------------------------------------------------------------------------*/
	WWWritePrivateProfileString("SerialDefaults", "CallWaitString",
		SerialDefaults.CallWaitString, buffer);
	WWWritePrivateProfileInt ("SerialDefaults", "CallWaitStringIndex", SerialDefaults.CallWaitStringIndex, buffer);
	WWWritePrivateProfileInt ("SerialDefaults", "InitStringIndex", SerialDefaults.InitStringIndex, buffer);
	WWWritePrivateProfileInt ("SerialDefaults", "Init", SerialDefaults.Init, buffer);
	WWWritePrivateProfileString("SerialDefaults", "DialMethod",
		DialMethodCheck[ SerialDefaults.DialMethod ], buffer);
	WWWritePrivateProfileInt ("SerialDefaults", "Baud", SerialDefaults.Baud, buffer);
	WWWritePrivateProfileInt ("SerialDefaults", "IRQ", SerialDefaults.IRQ, buffer);
	sprintf(buf, "%x", SerialDefaults.Port);
	WWWritePrivateProfileString("SerialDefaults", "Port", buf, buffer);
	WWWritePrivateProfileString("SerialDefaults", "ModemName", SerialDefaults.ModemName, buffer);
	WWWritePrivateProfileInt ("SerialDefaults", "Compression", SerialDefaults.Compression , buffer);
	WWWritePrivateProfileInt ("SerialDefaults", "ErrorCorrection", SerialDefaults.ErrorCorrection, buffer);
	WWWritePrivateProfileInt ("SerialDefaults", "HardwareFlowControl", SerialDefaults.HardwareFlowControl, buffer);

	/*------------------------------------------------------------------------
	Clear all existing InitString entries.
	------------------------------------------------------------------------*/
	WWWritePrivateProfileString ("InitStrings", NULL, NULL, buffer);

	/*------------------------------------------------------------------------
	Save all InitString entries.  In descending order so they come out in
	ascending order.
	------------------------------------------------------------------------*/
	for (i = (InitStrings.Count() - 1); i >= 0; i--) {
		sprintf( buf, "%03d", i);
		WWWritePrivateProfileString ("InitStrings", buf, InitStrings[i], buffer);
	}

	/*------------------------------------------------------------------------
	Clear all existing Phone Book entries.
	------------------------------------------------------------------------*/
	WWWritePrivateProfileString ("PhoneBook", NULL, NULL, buffer);

	/*------------------------------------------------------------------------
	Save all Phone Book entries.
	Format: Entry=Name,PhoneNum,Port,IRQ,Baud,InitString
	------------------------------------------------------------------------*/
	for (i = (PhoneBook.Count() - 1); i >= 0; i--) {
		snprintf(buf,sizeof(buf),"%s|%s|%x|%d|%d|%d|%d|%d|%s|%d|%d|%s",
			PhoneBook[i]->Name,
			PhoneBook[i]->Number,
			PhoneBook[i]->Settings.Port,
			PhoneBook[i]->Settings.IRQ,
			PhoneBook[i]->Settings.Baud,
			PhoneBook[i]->Settings.Compression,
			PhoneBook[i]->Settings.ErrorCorrection,
			PhoneBook[i]->Settings.HardwareFlowControl,
			DialMethodCheck[ PhoneBook[i]->Settings.DialMethod ],
			PhoneBook[i]->Settings.InitStringIndex,
			PhoneBook[i]->Settings.CallWaitStringIndex,
			PhoneBook[i]->Settings.CallWaitString);
		sprintf( entrytext, "%03d", i );
		WWWritePrivateProfileString ("PhoneBook", entrytext, buf, buffer);
	}

	/*------------------------------------------------------------------------
	Write the INI data out to a file.
	------------------------------------------------------------------------*/
	file.Open(WRITE);
	file.Write(buffer,strlen(buffer));
	file.Close();
}


/***********************************************************************************************
 * Read_Scenario_Descriptions -- reads multi-player scenario #'s # descriptions                *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1995 BR : Created.                                                                  *
 *=============================================================================================*/
void Read_Scenario_Descriptions (void)
{
	char *buffer;							// INI staging buffer pointer.
	CCFileClass file;
	int i;
	char fname[520];

	/*------------------------------------------------------------------------
	Clear the scenario description lists
	------------------------------------------------------------------------*/
	MPlayerScenarios.Clear();
	MPlayerFilenum.Clear();

	/*------------------------------------------------------------------------
	Loop through all possible scenario numbers; if a file is available, add
	its number to the FileNum list.
	------------------------------------------------------------------------*/
	for (i = 0; i < 100; i++) {
		Set_Scenario_Name(ScenarioName, i, SCEN_PLAYER_MPLAYER,
			SCEN_DIR_EAST, SCEN_VAR_A);
		snprintf(fname,sizeof(fname),"%s.INI",ScenarioName);
		file.Set_Name (fname);

		if (file.Is_Available()) {
			MPlayerFilenum.Add(i);
		}
	}

	/*------------------------------------------------------------------------
	Now, for every file in the FileNum list, read in the INI file, and extract
	its description.
	------------------------------------------------------------------------*/
	for (i = 0; i < MPlayerFilenum.Count(); i++) {
		/*.....................................................................
		Fetch working pointer to the INI staging buffer. Make sure that the
		buffer is cleared out before proceeding.
		.....................................................................*/
		buffer = (char *)_ShapeBuffer;
		memset(buffer, '\0', _ShapeBufferSize);

		/*.....................................................................
		Create filename and read the file.
		.....................................................................*/
		Set_Scenario_Name(ScenarioName, MPlayerFilenum[i], SCEN_PLAYER_MPLAYER,
			SCEN_DIR_EAST, SCEN_VAR_A);
		snprintf(fname,sizeof(fname),"%s.INI",ScenarioName);
		file.Set_Name (fname);
		file.Read(buffer, _ShapeBufferSize-1);
		file.Close();

		/*.....................................................................
		Extract description & add it to the list.
		.....................................................................*/
		WWGetPrivateProfileString("Basic", "Name", "Nulls-Ville",
			MPlayerDescriptions[i], 40, buffer);
		MPlayerScenarios.Add(MPlayerDescriptions[i]);
	}
}


/***********************************************************************************************
 * Free_Scenario_Descriptions -- frees memory for the scenario descriptions                    *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   06/05/1995 BRR : Created.                                                                 *
 *=============================================================================================*/
void Free_Scenario_Descriptions(void)
{
	int i;

	/*------------------------------------------------------------------------
	Clear the scenario descriptions & filenames
	------------------------------------------------------------------------*/
	MPlayerScenarios.Clear();
	MPlayerFilenum.Clear();

	/*------------------------------------------------------------------------
	Clear the initstring entries
	------------------------------------------------------------------------*/
	for (i = 0; i < InitStrings.Count(); i++) {
		delete InitStrings[i];
	}
	InitStrings.Clear();

	/*------------------------------------------------------------------------
	Clear the dialing entries
	------------------------------------------------------------------------*/
	for (i = 0; i < PhoneBook.Count(); i++) {
		delete PhoneBook[i];
	}
	PhoneBook.Clear();
}


/***************************************************************************
 * Computer_Message -- "sends" a message from the computer                 *
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
 *   06/06/1995 BRR : Created.                                             *
 *=========================================================================*/
void Computer_Message(void)
{
	int color;
	char txt[160];
	HousesType house;
	HouseClass *ptr;

	/*------------------------------------------------------------------------
	Find the computer house that the message will be from
	------------------------------------------------------------------------*/
	for (house = HOUSE_MULTI1; house < (HOUSE_MULTI1 + MPlayerMax); house++) {
		ptr = HouseClass::As_Pointer(house);

		if (!ptr || ptr->IsHuman || ptr->IsDefeated) {
			continue;
		}

		/*.....................................................................
		Decode this house's color
		.....................................................................*/
		color = MPlayerTColors[ptr->RemapColor];

		/*.....................................................................
		We now have a 1/4 chance of echoing one of the human players' messages
		back.
		.....................................................................*/
		if (IRandom(0,3) == 2) {
			/*..................................................................
			Now we have a 1/3 chance of garbling the human message.
			..................................................................*/
			if (IRandom(0,2) == 1) {
				Garble_Message(LastMessage);
			}

			/*..................................................................
			Only add the message if there is one to add.
			..................................................................*/
			if (strlen(LastMessage)) {
				sprintf(txt,"%s %s",Text_String(TXT_FROM_COMPUTER),LastMessage);
				Messages.Add_Message(txt, color,
					TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_FULLSHADOW, 600, 0, 0);
			}
		} else {
			sprintf(txt,"%s %s",Text_String(TXT_FROM_COMPUTER),
				Text_String(TXT_COMP_MSG1 + IRandom(0,12)));
			Messages.Add_Message(txt, color,
				TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_FULLSHADOW, 600, 0, 0);
		}

		return;
	}
}


/***************************************************************************
 * Garble_Message -- "garbles" a message                                   *
 *                                                                         *
 * INPUT:                                                                  *
 *      buf      buffer to garble; stores output message                   *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      none.                                                              *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   06/06/1995 BRR : Created.                                             *
 *=========================================================================*/
static void Garble_Message(char *buf)
{
	char txt[80];
	char punct[20];		// for punctuation
	char *p;					// working ptr
	int numwords;			// # words in the phrase
	char *words[40];		// ptrs to various words in the phrase
	int i,j;

	/*------------------------------------------------------------------------
	Pull off any trailing punctuation
	------------------------------------------------------------------------*/
	p = buf + strlen(buf) - 1;
	while (1) {
		if (p < buf)
			break;
		if (p[0]=='!' || p[0]=='.' || p[0]=='?') {
			p--;
		} else {
			p++;
			break;
		}
		if (strlen(p) >= (sizeof(punct) - 1) ) {
			break;
		}
	}
	strcpy (punct, p);
	p[0] = 0;

	for (i = 0; i < 40; i++) {
		words[i] = NULL;
	}

	/*------------------------------------------------------------------------
	Copy the original buffer
	------------------------------------------------------------------------*/
	strcpy(txt,buf);

	/*------------------------------------------------------------------------
	Split it up into words
	------------------------------------------------------------------------*/
	p = strtok (txt, " ");
	numwords = 0;
	while (p) {
		words[numwords] = p;
		numwords++;
		p = strtok (NULL, " ");
	}

	/*------------------------------------------------------------------------
	Now randomly put the words back.  Don't use the real random-number
	generator, since different machines will have different LastMessage's,
	and will go out of sync.
	------------------------------------------------------------------------*/
	buf[0] = 0;
	for (i = 0; i < numwords; i++) {
		j = Sim_IRandom(0,numwords);
		if (words[j] == NULL) {		// this word has been used already
			i--;
			continue;
		}
		strcat(buf,words[j]);
		words[j] = NULL;
		if (i < numwords-1)
			strcat(buf," ");
	}
	strcat(buf,punct);
}


/***************************************************************************
 * Surrender_Dialog -- Prompts user for surrendering                       *
 *                                                                         *
 * INPUT:                                                                  *
 *      none.                                                              *
 *                                                                         *
 * OUTPUT:                                                                 *
 *      0 = user cancels, 1 = user wants to surrender.                     *
 *                                                                         *
 * WARNINGS:                                                               *
 *      none.                                                              *
 *                                                                         *
 * HISTORY:                                                                *
 *   07/05/1995 BRR : Created.                                             *
 *=========================================================================*/
int Surrender_Dialog(void)
{
	int factor			= (SeenBuff.Get_Width() == 320) ? 1 : 2;
	/*........................................................................
	Dialog & button dimensions
	........................................................................*/
	int d_dialog_w = 170*factor;											// dialog width
	int d_dialog_h = 53*factor;											// dialog height
	int d_dialog_x = ((SeenBuff.Get_Width() - d_dialog_w) / 2);				// centered x-coord
	int d_dialog_y = ((SeenBuff.Get_Height() - d_dialog_h) / 2);				// centered y-coord
	int d_dialog_cx = d_dialog_x + (d_dialog_w / 2);		// coord of x-center

	int d_txt6_h = 6*factor+1;												// ht of 6-pt text
	int d_margin = 5*factor;												// margin width/height
	int d_topmargin = 20*factor;											// top margin

	int d_ok_w = 45*factor;												// ok width
	int d_ok_h = 9*factor;													// ok height
	int d_ok_x = d_dialog_cx - d_ok_w - 5*factor;					// ok x
	int d_ok_y = d_dialog_y + d_dialog_h - d_ok_h - d_margin;	// ok y

	int d_cancel_w = 45*factor;											// cancel width
	int d_cancel_h = 9*factor;											// cancel height
	int d_cancel_x = d_dialog_cx + 5*factor;							// cancel x
	int d_cancel_y = d_dialog_y + d_dialog_h - d_cancel_h - d_margin; // cancel y


	/*........................................................................
	Button enumerations
	........................................................................*/
	enum {
		BUTTON_OK = 100,
		BUTTON_CANCEL,
	};

	/*........................................................................
	Redraw values: in order from "top" to "bottom" layer of the dialog
	........................................................................*/
	typedef enum {
		REDRAW_NONE = 0,
		REDRAW_BUTTONS,
		REDRAW_BACKGROUND,
		REDRAW_ALL = REDRAW_BACKGROUND
	} RedrawType;

	/*........................................................................
	Dialog variables
	........................................................................*/
	RedrawType display;							// requested redraw level
	bool process;									// loop while true
	KeyNumType input;
	int retcode;

	/*........................................................................
	Buttons
	........................................................................*/
	ControlClass *commands = NULL;		// the button list

	TextButtonClass okbtn (BUTTON_OK, TXT_OK,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_ok_x, d_ok_y, d_ok_w, d_ok_h);

	TextButtonClass cancelbtn (BUTTON_CANCEL, TXT_CANCEL,
		TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW,
		d_cancel_x, d_cancel_y, d_cancel_w, d_cancel_h);

	/*
	------------------------------- Initialize -------------------------------
	*/
	Set_Logic_Page(SeenBuff);

	/*
	......................... Create the button list .........................
	*/
	commands = &okbtn;
	cancelbtn.Add_Tail(*commands);

	/*
	-------------------------- Main Processing Loop --------------------------
	*/
	display = REDRAW_ALL;
	process = true;
	while (process) {

		/*
		** If we have just received input focus again after running in the background then
		** we need to redraw.
		*/
		if (AllSurfaces.SurfacesRestored){
			AllSurfaces.SurfacesRestored=FALSE;
			display=REDRAW_ALL;
		}

		/*
		........................ Invoke game callback .........................
		*/
		if (Main_Loop()) {
			retcode = 0;
			process = false;
		}

		/*
		...................... Refresh display if needed ......................
		*/
		if (display) {

			/*
			...................... Display the dialog box ......................
			*/
			Hide_Mouse();
			if (display >= REDRAW_BACKGROUND) {
				Dialog_Box(d_dialog_x, d_dialog_y, d_dialog_w, d_dialog_h);
				Draw_Caption(TXT_NONE, d_dialog_x, d_dialog_y, d_dialog_w);

				/*
				....................... Draw the captions .......................
				*/
				Fancy_Text_Print(Text_String(TXT_SURRENDER),
					d_dialog_cx, d_dialog_y + d_topmargin,
					CC_GREEN, TBLACK,
					TPF_CENTER | TPF_6PT_GRAD | TPF_USE_GRAD_PAL | TPF_NOSHADOW);
			}

			/*
			........................ Redraw the buttons ........................
			*/
			if (display >= REDRAW_BUTTONS) {
				commands->Flag_List_To_Redraw();
			}
			Show_Mouse();
			display = REDRAW_NONE;
		}

		/*
		........................... Get user input ............................
		*/
		input = commands->Input();

		/*
		............................ Process input ............................
		*/
		switch (input) {
			case (KN_RETURN):
			case (BUTTON_OK | KN_BUTTON):
				retcode = 1;
				process = false;
				break;

			case (KN_ESC):
			case (BUTTON_CANCEL | KN_BUTTON):
				retcode = 0;
				process = false;
				break;

			default:
				break;
		}
	}

	/*
	--------------------------- Redraw the display ---------------------------
	*/
	HiddenPage.Clear();
	Map.Flag_To_Redraw(true);
	Map.Render();

	return (retcode);
}
