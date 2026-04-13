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

#ifdef USE_RENDER_BRIDGE
    /// Expose menu state for bridge-native rendering.
    struct RenderState {
        bool active;
        int menu_x, menu_y, menu_w, menu_h;
        int item_height;
        int num_items;
        const char* item_names[5];
    };
    RenderState Get_Render_State() const;
#endif

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
