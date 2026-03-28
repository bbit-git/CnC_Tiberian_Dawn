#include "function.h"
#include "actmenu.h"
#include "sidebar.h"

#define BUTTON_REPAIR 101

ActionMenuClass ActionMenu;

ActionMenuClass::ActionMenuClass()
{
    IsActive = false;
    EatRelease = false;
    MenuX = 0;
    MenuY = 0;
    MenuW = 90;
    MenuH = 0;
    ItemHeight = 20;
    NumItems = 0;
    
    Items[0] = { "Guard", KN_G };
    Items[1] = { "Stop", KN_S };
    Items[2] = { "Scatter", KN_X };
    Items[3] = { "Formation", KN_F };
    Items[4] = { "Repair", (KeyNumType)(BUTTON_REPAIR | KN_BUTTON) };
    NumItems = 5;
    
    MenuH = NumItems * ItemHeight + 4;
}

void ActionMenuClass::Activate(int x, int y)
{
    IsActive = true;
    MenuX = x;
    MenuY = y;
    
    if (MenuX + MenuW > SeenBuff.Get_Width()) {
        MenuX = SeenBuff.Get_Width() - MenuW;
    }
    if (MenuY + MenuH > SeenBuff.Get_Height()) {
        MenuY = SeenBuff.Get_Height() - MenuH;
    }
}

void ActionMenuClass::Deactivate()
{
    IsActive = false;
}

bool ActionMenuClass::AI(KeyNumType & input, int x, int y)
{
    // Eat the release event that follows a menu-item tap
    if (EatRelease) {
        int lo = input & 0xFF;
        if (lo == (KN_LMOUSE & 0xFF) || lo == (KN_RMOUSE & 0xFF) || lo == (KN_ACTION_MENU & 0xFF)) {
            EatRelease = false;
            input = KN_NONE;
            return true;
        }
    }

    if (!IsActive) {
        if (input == KN_ACTION_MENU) {
            Activate(x, y);
            input = KN_NONE;
            return true;
        }
        return false;
    }

    if (input == KN_LMOUSE || input == KN_RMOUSE || input == KN_ACTION_MENU) {
        if (x >= MenuX && x <= MenuX + MenuW && y >= MenuY && y <= MenuY + MenuH) {
            int idx = (y - MenuY - 2) / ItemHeight;
            if (idx >= 0 && idx < NumItems) {
                Stuff_Key_Num(Items[idx].Key);
                Stuff_Key_Num(Items[idx].Key | KN_RLSE_BIT);
            }
        }
        Deactivate();
        EatRelease = true;
        input = KN_NONE;
        return true;
    }

    if (input == KN_ESC) {
        Deactivate();
        input = KN_NONE;
        return true;
    }

    // Consume release events and other click variants while menu is open
    {
        int lo = input & 0xFF;
        if (lo == (KN_LMOUSE & 0xFF) || lo == (KN_RMOUSE & 0xFF) || lo == (KN_ACTION_MENU & 0xFF)) {
            input = KN_NONE;
            return true;
        }
    }

    return false;
}

void ActionMenuClass::Draw_It()
{
    if (!IsActive) return;

    // Background
    LogicPage->Fill_Rect(MenuX, MenuY, MenuX + MenuW, MenuY + MenuH, BLACK);
    LogicPage->Draw_Rect(MenuX, MenuY, MenuX + MenuW, MenuY + MenuH, WHITE);

    for (int i = 0; i < NumItems; i++) {
        int textX = MenuX + 5;
        int textY = MenuY + 2 + (i * ItemHeight) + 2;
        
        Fancy_Text_Print(Items[i].Name, textX, textY, WHITE, TBLACK, (TextPrintType)(TPF_6POINT | TPF_NOSHADOW));
    }
}
