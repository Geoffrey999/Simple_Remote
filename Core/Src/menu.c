#include "menu.h"
#include "oled.h"
#include "key.h"
#include <stdio.h>

static Menu_t Main_Menu;
static Menu_t PID_Menu;

Page_State_t Current_Page = PAGE_HOME;

static uint8_t AUX_Cursor = 0;
static uint8_t AUX_Editing = 0;
static uint8_t Offset_Cursor = 0;
static uint8_t Offset_Editing = 0;

static void Info_Callback(void);
static void Aux3_5_PID_Callback(void);
static void Aux6_8_PID_Callback(void);
static void Offset_Callback(void);
static void Attitude_Callback(void);

static Menu_Item_t Main_Items[] = {
    {"Attitude", 0,            Attitude_Callback},
    {"Offset",   0,            Offset_Callback},
    {"AUXs",      &PID_Menu,    0},
    {"Info",     0,            Info_Callback},

};

static Menu_Item_t PID_Items[] = {
    {"Aux3-5",    0,        Aux3_5_PID_Callback},
    {"Aux6-8",     0,        Aux6_8_PID_Callback},
};

Menu_t* Current_Menu = 0;

void Menu_Init(void)
{
    Main_Menu.Items = Main_Items;
    Main_Menu.Item_Count = 4;
    Main_Menu.Cursor = 0;
    Main_Menu.Scroll_Offset = 0;
    Main_Menu.Parent_Menu = 0;

    PID_Menu.Items = PID_Items;
    PID_Menu.Item_Count = 2;
    PID_Menu.Cursor = 0;
    PID_Menu.Scroll_Offset = 0;
    PID_Menu.Parent_Menu = &Main_Menu;

    Current_Page = PAGE_HOME;
    Current_Menu = &Main_Menu;
}

#define MAX_VISIBLE_ITEMS 4

void Home_Page_Render(void)
{
    char buf[20];

    OLED_Clear_Frame_Buffer();

    sprintf(buf, "Rol:%d", latest_tx_frame.roll);
    OLED_ShowString(0, 0, buf, 8, 0);

    sprintf(buf, "Pit:%d", latest_tx_frame.pitch);
    OLED_ShowString(0, 16, buf, 8, 0);

    sprintf(buf, "Yaw:%d", latest_tx_frame.yaw);
    OLED_ShowString(0, 32, buf, 8, 0);

    sprintf(buf, "Tho:%d", latest_tx_frame.throttle);
    OLED_ShowString(0, 48, buf, 8, 0);

	if(latest_tx_frame.arm==2000)OLED_ShowString(70, 0, "Unlocked", 8, 0);
	if(latest_tx_frame.arm!=2000)OLED_ShowString(70, 0, "Locked", 8, 0);

	if(latest_tx_frame.mode==1000)OLED_ShowString(70, 20, "Ang&Alt", 8, 0);
	if(latest_tx_frame.mode==1500)OLED_ShowString(70, 20, "Landing", 8, 0);
	if(latest_tx_frame.mode==2000)OLED_ShowString(70, 20, "Angle", 8, 0);

	sprintf(buf, "Bat:%d", Remote_Bat);
	OLED_ShowString(70, 40, buf, 8, 0);

    OLED_Refresh();
}

void Info_Page_Render(void)
{
    char buf[20];
    uint32_t runtime = HAL_GetTick();
    uint32_t seconds = runtime / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;

    OLED_Clear_Frame_Buffer();

    OLED_ShowString(0, 0, "Version: V0.1", 8, 0);
    OLED_ShowString(0, 16, "Author: Geoffrey999", 8, 0);

    sprintf(buf, "RunTime: %02d:%02d:%02d", hours % 24, minutes % 60, seconds % 60);
    OLED_ShowString(0, 32, buf, 8, 0);

    OLED_ShowString(0, 48, "Press BACK to exit", 8, 0);

    OLED_Refresh();
}

static void Info_Callback(void)
{
    Current_Page = PAGE_INFO;
}

static void Aux3_5_PID_Callback(void)
{
    Current_Page = PAGE_ANGLE_PID;
    AUX_Cursor = 0;
    AUX_Editing = 0;
}

static void Aux6_8_PID_Callback(void)
{
    Current_Page = PAGE_RATE_PID;
    AUX_Cursor = 0;
    AUX_Editing = 0;
}

static void Offset_Callback(void)
{
    Current_Page = PAGE_OFFSET;
    Offset_Cursor = 0;
    Offset_Editing = 0;
}

static void Attitude_Callback(void)
{
    Current_Page = PAGE_ATTITUDE;
}

void Attitude_Page_Render(void)
{
    char buf[48];

    OLED_Clear_Frame_Buffer();

    sprintf(buf, "Rol:%d", (int16_t)latest_rx_frame.roll);
    OLED_ShowString(0, 0, buf, 8, 0);

    sprintf(buf, "Alt:%5.2f", latest_rx_frame.altitude);
    OLED_ShowString(48, 0, buf, 8, 0);

    sprintf(buf, "Pit:%d", (int16_t)latest_rx_frame.pitch);
    OLED_ShowString(0, 16, buf, 8, 0);

    sprintf(buf, "Yaw:%d", (int16_t)latest_rx_frame.yaw);
    OLED_ShowString(0, 32, buf, 8, 0);

	if(latest_rx_frame.bat<=100 && latest_rx_frame.rssi<=100)
	{
		sprintf(buf, "Bat:%d RSSI:%d", latest_rx_frame.bat, latest_rx_frame.rssi);
		OLED_ShowString(0, 48, buf, 8, 0);
	}
	else
	{
		sprintf(buf, "Bat:-- RSSI:--");
		OLED_ShowString(0, 48, buf, 8, 0);
	}

    OLED_Refresh();
}

void Offset_Page_Render(void)
{
    char buf[20];

    OLED_Clear_Frame_Buffer();

    if(Offset_Cursor == 0)
        OLED_ShowString(0, 0, (Offset_Editing ? "*Rol:" : ">Rol:"), 8, 0);
    else
        OLED_ShowString(0, 0, " Rol:", 8, 0);
    sprintf(buf, "%d", menu_settings.roll_offset);
    OLED_ShowString(40, 0, buf, 8, 0);

    if(Offset_Cursor == 1)
        OLED_ShowString(0, 16, (Offset_Editing ? "*Pit:" : ">Pit:"), 8, 0);
    else
        OLED_ShowString(0, 16, " Pit:", 8, 0);
    sprintf(buf, "%d", menu_settings.pitch_offset);
    OLED_ShowString(40, 16, buf, 8, 0);

    OLED_ShowString(0, 48, "UP/DN:Sel EN:Edit", 8, 0);

    OLED_Refresh();
}

void Angle_PID_Page_Render(void)
{
    char buf[20];

    OLED_Clear_Frame_Buffer();

    if(AUX_Cursor == 0)
        OLED_ShowString(0, 0, (AUX_Editing ? "*A3:" : ">A3:"), 8, 0);
    else
        OLED_ShowString(0, 0, " A3:", 8, 0);
    sprintf(buf, "%d", menu_settings.aux3);
    OLED_ShowString(24, 0, buf, 8, 0);

    if(AUX_Cursor == 1)
        OLED_ShowString(64, 0, (AUX_Editing ? "*A4:" : ">A4:"), 8, 0);
    else
        OLED_ShowString(64, 0, " A4:", 8, 0);
    sprintf(buf, "%d", menu_settings.aux4);
    OLED_ShowString(88, 0, buf, 8, 0);

    if(AUX_Cursor == 2)
        OLED_ShowString(0, 16, (AUX_Editing ? "*A5:" : ">A5:"), 8, 0);
    else
        OLED_ShowString(0, 16, " A5:", 8, 0);
    sprintf(buf, "%d", menu_settings.aux5);
    OLED_ShowString(24, 16, buf, 8, 0);

    OLED_ShowString(0, 48, "UP/DN:Sel EN:Edit", 8, 0);

    OLED_Refresh();
}

void Rate_PID_Page_Render(void)
{
    char buf[20];

    OLED_Clear_Frame_Buffer();

    if(AUX_Cursor == 0)
        OLED_ShowString(0, 0, (AUX_Editing ? "*A6:" : ">A6:"), 8, 0);
    else
        OLED_ShowString(0, 0, " A6:", 8, 0);
    sprintf(buf, "%d", menu_settings.aux6);
    OLED_ShowString(24, 0, buf, 8, 0);

    if(AUX_Cursor == 1)
        OLED_ShowString(64, 0, (AUX_Editing ? "*A7:" : ">A7:"), 8, 0);
    else
        OLED_ShowString(64, 0, " A7:", 8, 0);
    sprintf(buf, "%d", menu_settings.aux7);
    OLED_ShowString(88, 0, buf, 8, 0);

    if(AUX_Cursor == 2)
        OLED_ShowString(0, 16, (AUX_Editing ? "*A8:" : ">A8:"), 8, 0);
    else
        OLED_ShowString(0, 16, " A8:", 8, 0);
    sprintf(buf, "%d", menu_settings.aux8);
    OLED_ShowString(24, 16, buf, 8, 0);

    OLED_ShowString(0, 48, "UP/DN:Sel EN:Edit", 8, 0);

    OLED_Refresh();
}

void Menu_Render(void)
{
    uint8_t i;
    uint8_t start_row;
    uint8_t visible_index;

    OLED_Clear_Frame_Buffer();

    for(i = 0; i < MAX_VISIBLE_ITEMS; i++)
    {
        visible_index = Current_Menu->Scroll_Offset + i;
        if(visible_index >= Current_Menu->Item_Count) break;

        start_row = i * 16;

        if(visible_index == Current_Menu->Cursor)
        {
            OLED_ShowString(0, start_row, ">", 8, 0);
            OLED_ShowString(8+4, start_row, Current_Menu->Items[visible_index].String, 8, 0);
        }
        else
        {
            OLED_ShowString(8, start_row, Current_Menu->Items[visible_index].String, 8, 0);
        }
    }
    OLED_Refresh();
}

void Menu_Enter(void)
{
    Menu_Item_t* item = &Current_Menu->Items[Current_Menu->Cursor];

    if(item->Child_Menu != 0)
    {
        Current_Menu = item->Child_Menu;
        Current_Menu->Cursor = 0;
        Current_Menu->Scroll_Offset = 0;
    }
    else if(item->Callback != 0)
    {
        item->Callback();
    }
}

void Menu_Back(void)
{
    if(Current_Menu->Parent_Menu != 0)
    {
        Current_Menu = Current_Menu->Parent_Menu;
    }
}

void Menu_Task_Handler(void)
{
    //Receive key event from InputTask (non-blocking poll)
    uint8_t key_pressed = KEY_NONE;
    osMessageQueueGet(KeyEventQueue, &key_pressed, 0, 0);

    uint16_t* aux_values;

    if(Current_Page == PAGE_HOME)
    {
        if(key_pressed == KEY_ENTER)
        {
            Current_Page = PAGE_MENU;
            Current_Menu = &Main_Menu;
            Current_Menu->Cursor = 0;
            Current_Menu->Scroll_Offset = 0;
        }
        Home_Page_Render();
        return;
    }

    if(Current_Page == PAGE_INFO)
    {
        if(key_pressed == KEY_BACK)
        {
            Current_Page = PAGE_MENU;
        }
        Info_Page_Render();
        return;
    }

    if(Current_Page == PAGE_ATTITUDE)
    {
        if(key_pressed == KEY_BACK)
        {
            Current_Page = PAGE_MENU;
        }
        Attitude_Page_Render();
        return;
    }

    if(Current_Page == PAGE_ANGLE_PID || Current_Page == PAGE_RATE_PID)
    {
        if(Current_Page == PAGE_ANGLE_PID)
            aux_values = &menu_settings.aux3;
        else
            aux_values = &menu_settings.aux6;

        switch(key_pressed)
        {
            case KEY_UP:
                if(AUX_Editing)
                {
                    if(aux_values[AUX_Cursor] < 65535)
                        aux_values[AUX_Cursor]++;
                }
                else
                {
                    if(AUX_Cursor > 0)
                        AUX_Cursor--;
                    else
                        AUX_Cursor = 2;
                }
                break;

            case KEY_UP_HOLD:
                if(AUX_Editing)
                {
                    if(aux_values[AUX_Cursor] < 65535 - 10)
                        aux_values[AUX_Cursor] += 10;
                    else
                        aux_values[AUX_Cursor] = 65535;
                }
                break;

            case KEY_DOWN:
                if(AUX_Editing)
                {
                    if(aux_values[AUX_Cursor] > 0)
                        aux_values[AUX_Cursor]--;
                }
                else
                {
                    if(AUX_Cursor < 2)
                        AUX_Cursor++;
                    else
                        AUX_Cursor = 0;
                }
                break;

            case KEY_DOWN_HOLD:
                if(AUX_Editing)
                {
                    if(aux_values[AUX_Cursor] >= 10)
                        aux_values[AUX_Cursor] -= 10;
                    else
                        aux_values[AUX_Cursor] = 0;
                }
                break;

            case KEY_ENTER:
                AUX_Editing = !AUX_Editing;
                break;

            case KEY_BACK:
                if(AUX_Editing)
                {
                    AUX_Editing = 0;
                }
                else
                {
                    Current_Page = PAGE_MENU;
                    Current_Menu = &PID_Menu;
                }
                break;

            default:
                break;
        }

        if(Current_Page == PAGE_ANGLE_PID)
            Angle_PID_Page_Render();
        else
            Rate_PID_Page_Render();
        return;
    }

    if(Current_Page == PAGE_OFFSET)
    {
        uint16_t* offset_values = &menu_settings.roll_offset;

        switch(key_pressed)
        {
            case KEY_UP:
                if(Offset_Editing)
                {
                    if(offset_values[Offset_Cursor] < 65535)
                        offset_values[Offset_Cursor]++;
                }
                else
                {
                    if(Offset_Cursor > 0)
                        Offset_Cursor--;
                    else
                        Offset_Cursor = 1;
                }
                break;

            case KEY_UP_HOLD:
                if(Offset_Editing)
                {
                    if(offset_values[Offset_Cursor] < 65535 - 10)
                        offset_values[Offset_Cursor] += 10;
                    else
                        offset_values[Offset_Cursor] = 65535;
                }
                break;

            case KEY_DOWN:
                if(Offset_Editing)
                {
                    if(offset_values[Offset_Cursor] > 0)
                        offset_values[Offset_Cursor]--;
                }
                else
                {
                    if(Offset_Cursor < 1)
                        Offset_Cursor++;
                    else
                        Offset_Cursor = 0;
                }
                break;

            case KEY_DOWN_HOLD:
                if(Offset_Editing)
                {
                    if(offset_values[Offset_Cursor] >= 10)
                        offset_values[Offset_Cursor] -= 10;
                    else
                        offset_values[Offset_Cursor] = 0;
                }
                break;

            case KEY_ENTER:
                Offset_Editing = !Offset_Editing;
                break;

            case KEY_BACK:
                if(Offset_Editing)
                {
                    Offset_Editing = 0;
                }
                else
                {
                    Current_Page = PAGE_MENU;
                    Current_Menu = &Main_Menu;
                }
                break;

            default:
                break;
        }

        Offset_Page_Render();
        return;
    }

    switch(key_pressed)
    {
        case KEY_UP:
            if(Current_Menu->Cursor > 0)
            {
                Current_Menu->Cursor--;
                if(Current_Menu->Cursor < Current_Menu->Scroll_Offset)
                {
                    Current_Menu->Scroll_Offset = Current_Menu->Cursor;
                }
            }
            else
            {
                Current_Menu->Cursor = Current_Menu->Item_Count - 1;
                if(Current_Menu->Item_Count > MAX_VISIBLE_ITEMS)
                {
                    Current_Menu->Scroll_Offset = Current_Menu->Item_Count - MAX_VISIBLE_ITEMS;
                }
            }
            break;

        case KEY_DOWN:
            if(Current_Menu->Cursor < Current_Menu->Item_Count - 1)
            {
                Current_Menu->Cursor++;
                if(Current_Menu->Cursor >= Current_Menu->Scroll_Offset + MAX_VISIBLE_ITEMS)
                {
                    Current_Menu->Scroll_Offset = Current_Menu->Cursor - MAX_VISIBLE_ITEMS + 1;
                }
            }
            else
            {
                Current_Menu->Cursor = 0;
                Current_Menu->Scroll_Offset = 0;
            }
            break;

        case KEY_ENTER:
            Menu_Enter();
            break;

        case KEY_BACK:
            if(Current_Menu->Parent_Menu == 0)
            {
                Current_Page = PAGE_HOME;
            }
            else
            {
                Menu_Back();
            }
            break;

        default:
            break;
    }

    if(Current_Page == PAGE_HOME)
    {
        Home_Page_Render();
    }
    else if(Current_Page == PAGE_INFO)
    {
        Info_Page_Render();
    }
    else if(Current_Page == PAGE_ATTITUDE)
    {
        Attitude_Page_Render();
    }
    else if(Current_Page == PAGE_ANGLE_PID)
    {
        Angle_PID_Page_Render();
    }
    else if(Current_Page == PAGE_RATE_PID)
    {
        Rate_PID_Page_Render();
    }
    else if(Current_Page == PAGE_OFFSET)
    {
        Offset_Page_Render();
    }
    else
    {
        Menu_Render();
    }
}
