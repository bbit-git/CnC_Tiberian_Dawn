#ifndef ACTMENU_H
#define ACTMENU_H

#include "function.h"

class ActionMenuClass {
public:
    ActionMenuClass();
    
    void Activate(int x, int y);
    void Deactivate();
    bool Is_Active() const { return IsActive; }
    
    bool AI(KeyNumType & input, int x, int y);
    void Draw_It();

private:
    bool IsActive;
    bool EatRelease;
    int MenuX;
    int MenuY;
    int MenuW;
    int MenuH;
    int ItemHeight;
    
    struct MenuItem {
        const char * Name;
        KeyNumType Key;
    };
    
    static const int MAX_ITEMS = 5;
    MenuItem Items[MAX_ITEMS];
    int NumItems;
};

extern ActionMenuClass ActionMenu;

#endif
