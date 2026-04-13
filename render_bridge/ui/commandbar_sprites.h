/**
 * commandbar_sprites.h — Sprite name constants for the HD command bar atlas.
 *
 * Maps legacy sidebar elements to their atlas sprite names in
 * MT_COMMANDBAR_COMMON.MTD. Created from the M1 sprite catalog dump
 * (docs/commandbar-sprite-catalog.td.txt).
 *
 * Naming convention: all names end with .TGA suffix as stored in the atlas.
 */

#ifndef CNC_COMMANDBAR_SPRITES_H
#define CNC_COMMANDBAR_SPRITES_H

// ---------------------------------------------------------------------------
// Sidebar chrome / background
// ---------------------------------------------------------------------------

/// Full sidebar build bar background (787×1212 px).
/// Replaces SIDE1.SHP/SIDE2.SHP tiling.
#define ATLAS_SIDEBAR_BUILDBARBG            "UI_SIDEBAR_BUILDBARBG.TGA"

/// Sell/repair button row background (868×107 px).
#define ATLAS_SIDEBAR_SELLREPAIRBG          "UI_SIDEBAR_SELLREPAIRBG.TGA"

/// Top button bar across sidebar top (868×82 px).
#define ATLAS_SIDEBAR_TOPBUTTON             "UI_SIDEBAR_TOPBUTTON.TGA"

/// Header bar segments: left cap, middle tile, right cap (32×84 / 797×84 / 32×84).
#define ATLAS_SIDEBAR_BUTTONHEADER_LEFT     "UI_SIDEBAR_BUTTONHEADER_LEFT.TGA"
#define ATLAS_SIDEBAR_BUTTONHEADER_MID      "UI_SIDEBAR_BUTTONHEADER_MID.TGA"
#define ATLAS_SIDEBAR_BUTTONHEADER_RIGHT    "UI_SIDEBAR_BUTTONHEADER_RIGHT.TGA"

// ---------------------------------------------------------------------------
// Build frame / slot
// ---------------------------------------------------------------------------

/// Individual build slot frame (118×88 px).
#define ATLAS_SIDEBAR_BUILDFRAME            "UI_SIDEBAR_BUILDFRAME.TGA"

// ---------------------------------------------------------------------------
// Build tab icon buttons (category selector bar)
// ---------------------------------------------------------------------------

/// Tab button backgrounds (3-part: left=170×67, mid=175×67, right=169×67).
#define ATLAS_SIDEBAR_TABICONBTN_LEFT       "UI_SIDEBAR_BUILDTABICONBUTTON_LEFT.TGA"
#define ATLAS_SIDEBAR_TABICONBTN_LEFT_ON    "UI_SIDEBAR_BUILDTABICONBUTTON_LEFT_ON.TGA"
#define ATLAS_SIDEBAR_TABICONBTN_MID        "UI_SIDEBAR_BUILDTABICONBUTTON_MID.TGA"
#define ATLAS_SIDEBAR_TABICONBTN_MID_ON     "UI_SIDEBAR_BUILDTABICONBUTTON_MID_ON.TGA"
#define ATLAS_SIDEBAR_TABICONBTN_RIGHT      "UI_SIDEBAR_BUILDTABICONBUTTON_RIGHT.TGA"
#define ATLAS_SIDEBAR_TABICONBTN_RIGHT_ON   "UI_SIDEBAR_BUILDTABICONBUTTON_RIGHT_ON.TGA"

/// Category tab icons (small glyphs inside tab buttons).
#define ATLAS_SIDEBAR_TABICON_STRUCTURE_OFF  "UI_SIDEBAR_TABICON_STRUCTURE_OFF.TGA"
#define ATLAS_SIDEBAR_TABICON_STRUCTURE_ON   "UI_SIDEBAR_TABICON_STRUCTURE_ON.TGA"
#define ATLAS_SIDEBAR_TABICON_STRUCTURE_BRIGHT "UI_SIDEBAR_TABICON_STRUCTURE_BRIGHT.TGA"
#define ATLAS_SIDEBAR_TABICON_INFANTRY_OFF   "UI_SIDEBAR_TABICON_INFANTRY_OFF.TGA"
#define ATLAS_SIDEBAR_TABICON_INFANTRY_ON    "UI_SIDEBAR_TABICON_INFANTRY_ON.TGA"
#define ATLAS_SIDEBAR_TABICON_INFANTRY_BRIGHT "UI_SIDEBAR_TABICON_INFANTRY_BRIGHT.TGA"
#define ATLAS_SIDEBAR_TABICON_VEHICLES_OFF   "UI_SIDEBAR_TABICON_VEHICLES_OFF.TGA"
#define ATLAS_SIDEBAR_TABICON_VEHICLES_ON    "UI_SIDEBAR_TABICON_VEHICLES_ON.TGA"
#define ATLAS_SIDEBAR_TABICON_VEHICLE_BRIGHT "UI_SIDEBAR_TABICON_VEHICLE_BRIGHT.TGA"
#define ATLAS_SIDEBAR_TABICON_SUPERS_OFF     "UI_SIDEBAR_TABICON_SUPERS_OFF.TGA"
#define ATLAS_SIDEBAR_TABICON_SUPERS_ON      "UI_SIDEBAR_TABICON_SUPERS_ON.TGA"
#define ATLAS_SIDEBAR_TABICON_SUPERS_BRIGHT  "UI_SIDEBAR_TABICON_SUPERS_BRIGHT.TGA"

/// Tab button sub-icons (small icons inside build tab buttons).
#define ATLAS_SIDEBAR_BUILDTABICON_STRUCTURE "UI_SIDEBAR_BUILDTABICONSTRUCTURE.TGA"
#define ATLAS_SIDEBAR_BUILDTABICON_INFANTRY  "UI_SIDEBAR_BUILDTABICONINFANTRY.TGA"
#define ATLAS_SIDEBAR_BUILDTABICON_VEHICLE   "UI_SIDEBAR_BUILDTABICONVEHICLE.TGA"
#define ATLAS_SIDEBAR_BUILDTABICON_SUPPORT   "UI_SIDEBAR_BUILDTABICONSUPPORT.TGA"

/// Tab button state backgrounds (187×87 px).
#define ATLAS_SIDEBAR_TABBUTTON_ENABLED      "UI_SIDEBAR_TABBUTTON_ENABLED.TGA"
#define ATLAS_SIDEBAR_TABBUTTON_DISABLED     "UI_SIDEBAR_TABBUTTON_DISABLED.TGA"
#define ATLAS_SIDEBAR_TABBUTTON_HIGHLIGHTED  "UI_SIDEBAR_TABBUTTON_HIGHLIGHTED.TGA"
#define ATLAS_SIDEBAR_TABBUTTON_PULSE        "UI_SIDEBAR_TABBUTTON_PULSE.TGA"

/// Generic button states (147×87 px).
#define ATLAS_SIDEBAR_BUTTON_ENABLED         "UI_SIDEBAR_BUTTON_ENABLED.TGA"
#define ATLAS_SIDEBAR_BUTTON_DISABLED        "UI_SIDEBAR_BUTTON_DISABLED.TGA"
#define ATLAS_SIDEBAR_BUTTON_HIGHLIGHTED     "UI_SIDEBAR_BUTTON_HIGHLIGHTED.TGA"
#define ATLAS_SIDEBAR_BUTTON_PULSE           "UI_SIDEBAR_BUTTON_PULSE.TGA"

// ---------------------------------------------------------------------------
// Category buttons (structures / infantry / vehicles / supers)
// ---------------------------------------------------------------------------

#define ATLAS_SIDEBAR_BTN_STRUCTURES_ON      "UI_SIDEBAR_BUTTON_STRUCTURES_ON.TGA"
#define ATLAS_SIDEBAR_BTN_STRUCTURES_OFF     "UI_SIDEBAR_BUTTON_STRUCTURES_OFF.TGA"
#define ATLAS_SIDEBAR_BTN_INFANTRY_ON        "UI_SIDEBAR_BUTTON_INFANTRY_ON.TGA"
#define ATLAS_SIDEBAR_BTN_INFANTRY_OFF       "UI_SIDEBAR_BUTTON_INFANTRY_OFF.TGA"
#define ATLAS_SIDEBAR_BTN_VEHICLES_ON        "UI_SIDEBAR_BUTTON_VEHICLES_ON.TGA"
#define ATLAS_SIDEBAR_BTN_VEHICLES_OFF       "UI_SIDEBAR_BUTTON_VEHICLES_OFF.TGA"
#define ATLAS_SIDEBAR_BTN_SUPERS_ON          "UI_SIDEBAR_BUTTON_SUPERS_ON.TGA"
#define ATLAS_SIDEBAR_BTN_SUPERS_OFF         "UI_SIDEBAR_BUTTON_SUPERS_OF.TGA"

// ---------------------------------------------------------------------------
// Repair / Sell / Map buttons — replaces REPAIR.SHP, SELL.SHP, MAP.SHP
// ---------------------------------------------------------------------------

#define ATLAS_SIDEBAR_BTN_REPAIR_OFF         "UI_SIDEBAR_BUTTON_REPAIR_OFF.TGA"
#define ATLAS_SIDEBAR_BTN_REPAIR_ON          "UI_SIDEBAR_BUTTON_REPAIR_ON.TGA"
#define ATLAS_SIDEBAR_BTN_REPAIR_HOVER       "UI_SIDEBAR_BUTTON_REPAIR_HOVER.TGA"
#define ATLAS_SIDEBAR_BTN_REPAIR_PRESSED     "UI_SIDEBAR_BUTTON_REPAIR_PRESSED.TGA"
#define ATLAS_SIDEBAR_BTN_REPAIR_PRESS       "UI_SIDEBAR_BUTTON_REPAIR_PRESS.TGA"

#define ATLAS_SIDEBAR_BTN_SELL_OFF           "UI_SIDEBAR_BUTTON_SELL_OFF.TGA"
#define ATLAS_SIDEBAR_BTN_SELL_ON            "UI_SIDEBAR_BUTTON_SELL_ON.TGA"
#define ATLAS_SIDEBAR_BTN_SELL_HOVER         "UI_SIDEBAR_BUTTON_SELL_HOVER.TGA"
#define ATLAS_SIDEBAR_BTN_SELL_PRESS         "UI_SIDEBAR_BUTTON_SELL_PRESS.TGA"
#define ATLAS_SIDEBAR_BTN_SELL_PRESSED       "UI_SIDEBAR_BUTTON_SELL_PRESSED.TGA"

#define ATLAS_SIDEBAR_BTN_MAP_OFF            "UI_SIDEBAR_BUTTON_MAP_OFF.TGA"
#define ATLAS_SIDEBAR_BTN_MAP_ON             "UI_SIDEBAR_BUTTON_MAP_ON.TGA"
#define ATLAS_SIDEBAR_BTN_MAP_HOVER          "UI_SIDEBAR_BUTTON_MAP_HOVER.TGA"
#define ATLAS_SIDEBAR_BTN_MAP_PRESS          "UI_SIDEBAR_BUTTON_MAP_PRESS.TGA"
#define ATLAS_SIDEBAR_BTN_MAP_PRESSED        "UI_SIDEBAR_BUTTON_MAP_PRESSED.TGA"

// ---------------------------------------------------------------------------
// Sell/Repair button bar (3-part stretch)
// ---------------------------------------------------------------------------

#define ATLAS_SIDEBAR_SELLREPAIRBTN_LEFT     "UI_SIDEBAR_SELLREPAIRBUTTON_LEFT.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_MID      "UI_SIDEBAR_SELLREPAIRBUTTON_MID.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_RIGHT    "UI_SIDEBAR_SELLREPAIRBUTTON_RIGHT.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_ON_LEFT  "UI_SIDEBAR_SELLREPAIRBUTTON_ON_LEFT.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_ON_MID   "UI_SIDEBAR_SELLREPAIRBUTTON_ON_MID.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_ON_EDGE  "UI_SIDEBAR_SELLREPAIRBUTTON_ON_EDGE.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_ON_RIGHT "UI_SIDEBAR_SELLREPAIRBUTTON_ON_RIGHT.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_HOVER_LEFT  "UI_SIDEBAR_SELLREPAIRBUTTON_HOVER_LEFT.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_HOVER_MID   "UI_SIDEBAR_SELLREPAIRBUTTON_HOVER_MID.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_HOVER_RIGHT "UI_SIDEBAR_SELLREPAIRBUTTON_HOVER_RIGHT.TGA"
#define ATLAS_SIDEBAR_SELLREPAIRBTN_PRESSED_MID "UI_SIDEBAR_SELLREPAIRBUTTON_PRESSED_MID.TGA"

// ---------------------------------------------------------------------------
// Power bar — replaces POWER.SHP
// ---------------------------------------------------------------------------

#define ATLAS_SIDEBAR_POWERBG                "UI_SIDEBAR_POWERBG.TGA"
#define ATLAS_SIDEBAR_POWERFRAMING           "UI_SIDEBAR_POWERFRAMING.TGA"
#define ATLAS_SIDEBAR_POWERMETEROFF          "UI_SIDEBAR_POWERMETEROFF.TGA"

// Note: power bar fill segments are standalone DDS files in the MEG, not in the atlas.
// UI_SIDEBAR_POWERSEGMENT_EMPTY.DDS, UI_SIDEBAR_POWERSEGMENT_FILLED.DDS

// ---------------------------------------------------------------------------
// Radar frame — replaces RADAR*.SHP
// ---------------------------------------------------------------------------

#define ATLAS_SIDEBAR_RADARBG                "UI_SIDEBAR_RADARBG.TGA"

// ---------------------------------------------------------------------------
// Faction logos (displayed in radar area when no radar)
// ---------------------------------------------------------------------------

#define ATLAS_SIDEBAR_FACTIONLOGO_GDI        "UI_SIDEBAR_FACTIONLOGO_GDI.TGA"
#define ATLAS_SIDEBAR_FACTIONLOGO_NOD        "UI_SIDEBAR_FACTIONLOGO_NOD.TGA"
#define ATLAS_SIDEBAR_FACTIONLOGO_DINO       "UI_SIDEBAR_FACTIONLOGO_DINO.TGA"

// ---------------------------------------------------------------------------
// Faction selector buttons (multiplayer)
// ---------------------------------------------------------------------------

#define ATLAS_SIDEBAR_BTN_FACTION_GDI_OFF    "UI_SIDEBAR_BUTTON_FACTION_GDI_OFF.TGA"
#define ATLAS_SIDEBAR_BTN_FACTION_GDI_ON     "UI_SIDEBAR_BUTTON_FACTION_GDI_ON.TGA"
#define ATLAS_SIDEBAR_BTN_FACTION_GDI_OVER   "UI_SIDEBAR_BUTTON_FACTION_GDI_OVER.TGA"
#define ATLAS_SIDEBAR_BTN_FACTION_NOD_OFF    "UI_SIDEBAR_BUTTON_FACTION_NOD_OFF.TGA"
#define ATLAS_SIDEBAR_BTN_FACTION_NOD_ON     "UI_SIDEBAR_BUTTON_FACTION_NOD_ON.TGA"
#define ATLAS_SIDEBAR_BTN_FACTION_NOD_OVER   "UI_SIDEBAR_BUTTON_FACTION_NOD_OVER.TGA"

// ---------------------------------------------------------------------------
// Utility buttons (minimize, maximize, ping, player, menu)
// ---------------------------------------------------------------------------

#define ATLAS_SIDEBAR_MINIMIZEBTN_OFF        "UI_SIDEBAR_MINIMIZEBUTTON_OFF.TGA"
#define ATLAS_SIDEBAR_MINIMIZEBTN_HOVER      "UI_SIDEBAR_MINIMIZEBUTTON_HOVER.TGA"
#define ATLAS_SIDEBAR_MINIMIZEBTN_PRESS      "UI_SIDEBAR_MINIMIZEBUTTON_PRESS.TGA"
#define ATLAS_SIDEBAR_MAXIMIZEBTN_OFF        "UI_SIDEBAR_MAXIMIZEBUTTON_OFF.TGA"
#define ATLAS_SIDEBAR_MAXIMIZEBTN_HOVER      "UI_SIDEBAR_MAXIMIZEBUTTON_HOVER.TGA"
#define ATLAS_SIDEBAR_MAXIMIZEBTN_PRESS      "UI_SIDEBAR_MAXIMIZEBUTTON_PRESS.TGA"
#define ATLAS_SIDEBAR_PINGBTN_OFF            "UI_SIDEBAR_PINGBUTTON_OFF.TGA"
#define ATLAS_SIDEBAR_PINGBTN_HOVER          "UI_SIDEBAR_PINGBUTTON_HOVER.TGA"
#define ATLAS_SIDEBAR_PINGBTN_PRESS          "UI_SIDEBAR_PINGBUTTON_PRESS.TGA"
#define ATLAS_SIDEBAR_PLAYERBTN_OFF          "UI_SIDEBAR_PLAYERBUTTON_OFF.TGA"
#define ATLAS_SIDEBAR_PLAYERBTN_HOVER        "UI_SIDEBAR_PLAYERBUTTON_HOVER.TGA"
#define ATLAS_SIDEBAR_PLAYERBTN_PRESS        "UI_SIDEBAR_PLAYERBUTTON_PRESS.TGA"
#define ATLAS_SIDEBAR_MENUBTN_OFF            "UI_SIDEBAR_MENUBUTTON_OFF.TGA"
#define ATLAS_SIDEBAR_MENUBTN_HOVER          "UI_SIDEBAR_MENUBUTTON_HOVER.TGA"
#define ATLAS_SIDEBAR_MENUBTN_PRESS          "UI_SIDEBAR_MENUBUTTON_PRESS.TGA"

// ---------------------------------------------------------------------------
// Build cameos — replaces {NAME}ICNH.SHP via BUILDICON_TD_{NAME}.TGA
// All are 341×256 px (flag=0).
// ---------------------------------------------------------------------------

// Format: BUILDICON_TD_{ININAME}.TGA
// The IniName-to-atlas mapping is done at runtime using the pattern:
//   "BUILDICON_TD_" + uppercase(ObjectTypeClass::IniName) + ".TGA"
// All 54 TD build icons follow this convention.
//
// Special weapons use hardcoded names:
//   Ion Cannon:     BUILDICON_TD_IONCANNON.TGA
//   Nuclear Strike: BUILDICON_TD_NUCLEARSTRIKE.TGA
//   Air Strike:     BUILDICON_TD_AIRSTRIKE.TGA

#endif // CNC_COMMANDBAR_SPRITES_H
