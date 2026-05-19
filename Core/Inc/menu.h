#ifndef __MENU_H__
#define __MENU_H__

#include "main.h"
#include "protocol.h"

typedef enum {
    PAGE_HOME,
    PAGE_MENU,
    PAGE_INFO,
    PAGE_ANGLE_PID,
    PAGE_RATE_PID,
    PAGE_OFFSET,
    PAGE_ATTITUDE
} Page_State_t;

typedef struct Menu Menu_t;
typedef struct Menu_Item Menu_Item_t;

typedef void (*Menu_Callback)(void);

struct Menu_Item {
    char* String;
    Menu_t* Child_Menu;
    Menu_Callback Callback;
};

struct Menu {
    Menu_Item_t* Items;
    uint8_t Item_Count;
    uint8_t Cursor;
    uint8_t Scroll_Offset;
    Menu_t* Parent_Menu;
};

extern Menu_t* Current_Menu;
extern Page_State_t Current_Page;

void Menu_Init(void);
void Menu_Task_Handler(void);
void Menu_Enter(void);
void Menu_Back(void);
void Menu_Render(void);
void Home_Page_Render(void);

#endif
