#include "menu.h"
#include "ui_core.h"
#include "ui_anim.h"
#include "ui_home.h"
#include "ui_menu.h"
#include "ui_edit.h"
#include "ui_info.h"
#include "ui_attitude.h"
#include "ui_widgets.h"
#include "key.h"
#include <stdio.h>

/* 信息页显示的固件版本，改这里就行 */
#define APP_VERSION     "V0.3"

/* 过渡动画时长（ms） */
#define NAV_DURATION_MS     180     /* 整页滑入/滑出 */
#define SEL_DURATION_MS     120     /* 菜单选择条移动 */

/* "油门没归零，拒绝解锁"提示框：动画时长与几何尺寸 */
#define ARM_WARN_MS         2000    /* 完整停留时长（从最后一次尝试算起） */
#define ARM_WARN_IN_MS      180     /* 进场动画 */
#define ARM_WARN_OUT_MS     150     /* 退场动画 */

#define ARM_WARN_X0         28
#define ARM_WARN_X1         99
#define ARM_WARN_Y          16
#define ARM_WARN_H          32                          /* 框高，y 范围 0..31 */
#define ARM_WARN_TRAVEL     (ARM_WARN_H + 16)           /* 进场位移：从完全在屏幕外开始 */

static Menu_t Main_Menu;
static Menu_t PID_Menu;

Page_State_t Current_Page = PAGE_HOME;

static uint8_t AUX_Cursor = 0;
static uint8_t AUX_Editing = 0;
static uint8_t Offset_Cursor = 0;
static uint8_t Offset_Editing = 0;

/* ---- 页面切换动画 ---- */
static UiAnim_t     Nav_Anim;
static uint8_t      Nav_Active;
static Page_State_t Nav_From;
static Page_State_t Nav_To;
static int8_t       Nav_Dir;

/* 菜单 -> 菜单（子菜单进出）：页面还是 PAGE_MENU，只换菜单对象 */
static uint8_t      Nav_Menu;
static Menu_t*      Nav_From_Menu;
static Menu_t*      Nav_To_Menu;

/* ---- 菜单选择条动画 ---- */
static UiAnim_t     Sel_Anim;
static int16_t      Sel_Y;          /* 当前像素位置 */
static int16_t      Sel_Y_From;
static int16_t      Sel_Y_To;

/* ---- 开机淡入 ---- */
#define BOOT_FADE_MS    1000
static UiAnim_t     Boot_Anim;
static uint8_t      Boot_Active;

/* ---- 解锁被拒绝的提示 ---- */
static volatile uint32_t Arm_Deny_Tick;     /* 最后一次被拒绝的时刻，0 = 从未发生 */

void Menu_Notify_ArmDenied(void)
{
    Arm_Deny_Tick = HAL_GetTick();
}

/* 弹框是否正在做进场/退场动画（用来把帧率提到 62fps） */
static uint8_t Arm_Warn_Animating(void)
{
    uint32_t elapsed;

    if (Arm_Deny_Tick == 0)
    {
        return 0;
    }

    elapsed = HAL_GetTick() - Arm_Deny_Tick;

    return (uint8_t)((elapsed < ARM_WARN_IN_MS) ||
                     ((elapsed >= ARM_WARN_MS) &&
                      (elapsed < (ARM_WARN_MS + ARM_WARN_OUT_MS))));
}

/*
 * 提示框：2px 白边框 + 黑底 + 白字，从屏幕上方滑入，2 秒后滑出。
 * 垂直方向只做平移，内容不会超出边框（1bpp 下没有裁剪区，平移最稳）。
 */
/* progress: 0 = 完全在屏幕上方，255 = 停在目标位置 */
static int16_t Arm_Warn_Y_For(uint8_t progress)
{
    return (int16_t)((ARM_WARN_Y - ARM_WARN_TRAVEL) +
                     ((int32_t)ARM_WARN_TRAVEL * (int32_t)progress) / 255);
}

static void Draw_Arm_Warning(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed;
    int16_t  y;
    uint8_t  e;

    if (Arm_Deny_Tick == 0)
    {
        return;
    }

    elapsed = now - Arm_Deny_Tick;

    if (elapsed < ARM_WARN_IN_MS)
    {
        /* 进场：从上方滑下来，缓出（快到位置时减速） */
        e = ui_ease_out((uint8_t)((elapsed * 255u) / ARM_WARN_IN_MS));
        y = Arm_Warn_Y_For(e);
    }
    else if (elapsed < ARM_WARN_MS)
    {
        /* 停留 */
        y = ARM_WARN_Y;
    }
    else if (elapsed < (ARM_WARN_MS + ARM_WARN_OUT_MS))
    {
        /* 退场：进度反向，缓出 → 先快后慢地滑回上方 */
        uint8_t t = (uint8_t)(((elapsed - ARM_WARN_MS) * 255u) / ARM_WARN_OUT_MS);

        e = (uint8_t)(255u - ui_ease_out(t));
        y = Arm_Warn_Y_For(e);
    }
    else
    {
        return;
    }

    ui_set_offset(0, 0);        /* 页面过渡期间也画在正中间 */

    /* 整块填白，再把中间掏空 —— 剩下的 2px 就是边框 */
    ui_fill_rect(ARM_WARN_X0, y, ARM_WARN_X1, (int16_t)(y + ARM_WARN_H - 1), UI_ON);
    ui_fill_rect((int16_t)(ARM_WARN_X0 + 2), (int16_t)(y + 2),
                 (int16_t)(ARM_WARN_X1 - 2), (int16_t)(y + ARM_WARN_H - 3), UI_OFF);

    ui_text(40, (int16_t)(y + 6),  "THROTTLE", UI_F_BODY);
    ui_text(40, (int16_t)(y + 18), "NOT AT 0", UI_F_BODY);
}

static void    Menu_Goto(Page_State_t page, int8_t dir);
static void    Menu_Goto_Menu(Menu_t* to, int8_t dir);
static void    Menu_Sel_Reset(void);
static int16_t Menu_Sel_Target(void);
static int16_t Menu_Sel_Y_For(const Menu_t* m);
static void    Menu_Render_For(const Menu_t* m, int16_t sel_y);

static void Info_Callback(void);
static void Aux3_5_PID_Callback(void);
static void Aux6_8_PID_Callback(void);
static void Offset_Callback(void);
static void Attitude_Callback(void);

/* 菜单里能改的都是直接发给飞控的通道值：读写统一走 protocol.c 的 Settings_*，
   由它负责加锁和 1000~2000 限幅。这里只维护"光标 -> 字段"的映射表。 */
static const SettingId_t Aux3_Ids[3] = { SETTING_AUX3, SETTING_AUX4, SETTING_AUX5 };
static const SettingId_t Aux6_Ids[3] = { SETTING_AUX6, SETTING_AUX7, SETTING_AUX8 };
static const SettingId_t Offset_Ids[2] = { SETTING_ROLL_OFFSET, SETTING_PITCH_OFFSET };

/* 显示用的帧快照：每次 Menu_Task_Handler 开头取一次，
   保证一屏内容来自同一帧，不会出现"上半屏新帧 + 下半屏旧帧"。 */
static Tx_Frame_t Display_Tx;
static Rx_Frame_t Display_Rx;

static void Display_Take_Snapshot(void)
{
    TxFrame_Latest_Get(&Display_Tx);
    RxFrame_Latest_Get(&Display_Rx);
}

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
    Main_Menu.Title = "MENU";
    Main_Menu.Items = Main_Items;
    Main_Menu.Item_Count = 4;
    Main_Menu.Cursor = 0;
    Main_Menu.Scroll_Offset = 0;
    Main_Menu.Parent_Menu = 0;

    PID_Menu.Title = "AUX";
    PID_Menu.Items = PID_Items;
    PID_Menu.Item_Count = 2;
    PID_Menu.Cursor = 0;
    PID_Menu.Scroll_Offset = 0;
    PID_Menu.Parent_Menu = &Main_Menu;

    Current_Page = PAGE_HOME;
    Current_Menu = &Main_Menu;

    Menu_Sel_Reset();

    /* 开机淡入：第一帧是全黑，1000ms 内抖动渐显 */
    Boot_Active = 1;
    UiAnim_Start(&Boot_Anim, HAL_GetTick(), BOOT_FADE_MS);
}

/* 一屏可见项数，必须和 ui_menu.c 的布局一致 */
#define MAX_VISIBLE_ITEMS UI_MENU_ROWS

void Home_Page_Render(void)
{
    UiHomeData_t data;

    data.roll     = Display_Tx.roll;
    data.pitch    = Display_Tx.pitch;
    data.yaw      = Display_Tx.yaw;
    data.throttle = Display_Tx.throttle;
    data.arm      = Display_Tx.arm;
    data.mode     = Display_Tx.mode;

    data.bat      = (Remote_Bat > 0) ? (uint8_t)Remote_Bat : 0;
    data.rx_valid = RxLink_IsAlive();
    data.rssi     = data.rx_valid ? (uint8_t)Display_Rx.rssi : 0;

    UI_Home_Render(&data);
}

void Info_Page_Render(void)
{
    UI_Info_Render(APP_VERSION, HAL_GetTick() / 1000u);
}

static void Info_Callback(void)
{
    Menu_Goto(PAGE_INFO, +1);
}

static void Aux3_5_PID_Callback(void)
{
    AUX_Cursor = 0;
    AUX_Editing = 0;
    Menu_Goto(PAGE_ANGLE_PID, +1);
}

static void Aux6_8_PID_Callback(void)
{
    AUX_Cursor = 0;
    AUX_Editing = 0;
    Menu_Goto(PAGE_RATE_PID, +1);
}

static void Offset_Callback(void)
{
    Offset_Cursor = 0;
    Offset_Editing = 0;
    Menu_Goto(PAGE_OFFSET, +1);
}

static void Attitude_Callback(void)
{
    Menu_Goto(PAGE_ATTITUDE, +1);
}

void Attitude_Page_Render(void)
{
    UiAttitudeData_t data;

    data.roll     = Display_Rx.roll;
    data.pitch    = Display_Rx.pitch;
    data.yaw      = Display_Rx.yaw;
    data.altitude = Display_Rx.altitude;
    data.bat      = Display_Rx.bat;
    data.link_ok  = RxLink_IsAlive();
    data.rssi     = data.link_ok ? Display_Rx.rssi : 0;

    UI_Attitude_Render(&data);
}

void Offset_Page_Render(void)
{
    UiField_t fields[2];

    fields[0].label = "ROL";
    fields[0].value = Settings_Get(SETTING_ROLL_OFFSET);
    fields[1].label = "PIT";
    fields[1].value = Settings_Get(SETTING_PITCH_OFFSET);

    UI_Edit_Render("OFFSET", fields, 2, Offset_Cursor, Offset_Editing);
}

void Angle_PID_Page_Render(void)
{
    UiField_t fields[3];

    fields[0].label = "A3";
    fields[0].value = Settings_Get(SETTING_AUX3);
    fields[1].label = "A4";
    fields[1].value = Settings_Get(SETTING_AUX4);
    fields[2].label = "A5";
    fields[2].value = Settings_Get(SETTING_AUX5);

    UI_Edit_Render("AUX 3-5", fields, 3, AUX_Cursor, AUX_Editing);
}

void Rate_PID_Page_Render(void)
{
    UiField_t fields[3];

    fields[0].label = "A6";
    fields[0].value = Settings_Get(SETTING_AUX6);
    fields[1].label = "A7";
    fields[1].value = Settings_Get(SETTING_AUX7);
    fields[2].label = "A8";
    fields[2].value = Settings_Get(SETTING_AUX8);

    UI_Edit_Render("AUX 6-8", fields, 3, AUX_Cursor, AUX_Editing);
}

/* ------------------------------------------------------------------------- */
/* 动画：页面滑动 + 选择条滑动                                                */
/* ------------------------------------------------------------------------- */

static int16_t Menu_Sel_Target(void)
{
    return (int16_t)(13 + (Current_Menu->Cursor - Current_Menu->Scroll_Offset) * UI_MENU_PITCH);
}

/* 某个菜单里，选择条应该停在哪一行（用于过渡中画"另一侧"的菜单） */
static int16_t Menu_Sel_Y_For(const Menu_t* m)
{
    return (int16_t)(13 + (m->Cursor - m->Scroll_Offset) * UI_MENU_PITCH);
}

/* 让选择条直接落位（进入/退出菜单时用，避免从旧位置飞过来） */
static void Menu_Sel_Reset(void)
{
    Sel_Y_To   = Menu_Sel_Target();
    Sel_Y_From = Sel_Y_To;
    Sel_Y      = Sel_Y_To;
}

/*
 * 切页：dir > 0 新页从右侧滑入（进下一层），dir < 0 从左侧滑入（返回）。
 * 逻辑页面立刻切换，画面在 NAV_DURATION_MS 内滑过去。
 */
static void Menu_Goto(Page_State_t page, int8_t dir)
{
    if (page == Current_Page)
    {
        return;
    }

    Nav_From   = Current_Page;
    Nav_To     = page;
    Nav_Dir    = dir;
    Nav_Active = 1;
    Nav_Menu   = 0;

    UiAnim_Start(&Nav_Anim, HAL_GetTick(), NAV_DURATION_MS);

    Current_Page = page;
}

/*
 * 子菜单进出：页面仍然是 PAGE_MENU，只是换一个菜单对象。
 * 两个菜单并排画在同一屏里，方向规则和整页滑动一致。
 */
static void Menu_Goto_Menu(Menu_t* to, int8_t dir)
{
    if (to == Current_Menu)
    {
        return;
    }

    Nav_From_Menu = Current_Menu;
    Nav_To_Menu   = to;
    Nav_Dir       = dir;
    Nav_Active    = 1;
    Nav_Menu      = 1;

    UiAnim_Start(&Nav_Anim, HAL_GetTick(), NAV_DURATION_MS);

    Current_Menu = to;
}

uint8_t Menu_Is_Animating(void)
{
    return (uint8_t)((Nav_Active != 0) || (Sel_Y != Sel_Y_To) ||
                     (Boot_Active != 0) || (Arm_Warn_Animating() != 0));
}

static void Menu_Update_Selection(uint32_t now)
{
    int16_t target = Menu_Sel_Target();

    if (target != Sel_Y_To)
    {
        Sel_Y_From = Sel_Y;
        Sel_Y_To   = target;
        UiAnim_Start(&Sel_Anim, now, SEL_DURATION_MS);
    }

    if (Sel_Y_From != Sel_Y_To)
    {
        uint8_t p = ui_ease_out(UiAnim_Progress(&Sel_Anim, now));

        Sel_Y = (int16_t)(Sel_Y_From + (((int32_t)(Sel_Y_To - Sel_Y_From) * p) / 255));
    }
    else
    {
        Sel_Y = Sel_Y_To;
    }
}

/* 只画页面内容，不清屏也不刷新 */
static void Draw_Page(Page_State_t page)
{
    switch (page)
    {
        case PAGE_HOME:      Home_Page_Render();      break;
        case PAGE_MENU:      Menu_Render();           break;
        case PAGE_INFO:      Info_Page_Render();      break;
        case PAGE_ATTITUDE:  Attitude_Page_Render();  break;
        case PAGE_ANGLE_PID: Angle_PID_Page_Render(); break;
        case PAGE_RATE_PID:  Rate_PID_Page_Render();  break;
        case PAGE_OFFSET:    Offset_Page_Render();    break;
        default: break;
    }
}

/* 过渡中"旧的一侧"：可能是另一个页面，也可能是同一个页面里的另一个菜单 */
static void Draw_From_Side(void)
{
    if (Nav_Menu)
    {
        Menu_Render_For(Nav_From_Menu, Menu_Sel_Y_For(Nav_From_Menu));
    }
    else
    {
        Draw_Page(Nav_From);
    }
}

static void Draw_To_Side(void)
{
    if (Nav_Menu)
    {
        Menu_Render_For(Nav_To_Menu, Menu_Sel_Y_For(Nav_To_Menu));
    }
    else
    {
        Draw_Page(Nav_To);
    }
}

static void Render_Frame(void)
{
    uint32_t now = HAL_GetTick();

    ui_begin();

    if (Nav_Active)
    {
        uint8_t progress = UiAnim_Progress(&Nav_Anim, now);
        int16_t offset   = (int16_t)(((int32_t)UI_W * ui_ease_out(progress)) / 255);

        if (Nav_Dir > 0)
        {
            ui_set_offset((int16_t)(-offset), 0);
            Draw_From_Side();
            ui_set_offset((int16_t)(UI_W - offset), 0);
            Draw_To_Side();
        }
        else
        {
            ui_set_offset(offset, 0);
            Draw_From_Side();
            ui_set_offset((int16_t)(offset - UI_W), 0);
            Draw_To_Side();
        }

        if (progress >= 255)
        {
            Nav_Active = 0;
            Nav_Menu   = 0;
        }
    }
    else
    {
        Draw_Page(Current_Page);
    }

    /* 开机淡入：整帧画完之后再按 Bayer 抖动遮罩压暗 */
    if (Boot_Active)
    {
        uint8_t p = UiAnim_Progress(&Boot_Anim, now);

        ui_dither_fade(ui_ease_out(p));

        if (p >= 255)
        {
            Boot_Active = 0;

            /* 淡入结束：重新下发一次面板配置并整屏重绘。
               万一上电那次初始化因为电源/时序问题没生效（表现为花屏或黑屏），
               这里会自动纠回来；正常情况下只是一次几毫秒的清屏+重画，看不出来。 */
            ui_resync_display();
        }
    }

    /* 解锁被拒绝时的提示框：盖在当前页面之上 */
    Draw_Arm_Warning();

    ui_flush();
}

/* 画指定菜单（过渡中两侧各自可能停在不同行，所以选择条位置单独传入） */
static void Menu_Render_For(const Menu_t* m, int16_t sel_y)
{
    const char* items[UI_MENU_ROWS + 1];
    uint8_t count = m->Item_Count;
    uint8_t i;

    if (count > UI_MENU_ROWS + 1)
    {
        count = UI_MENU_ROWS + 1;
    }

    for (i = 0; i < count; i++)
    {
        items[i] = m->Items[i].String;
    }

    UI_Menu_Render(m->Title, items, m->Item_Count, m->Cursor, m->Scroll_Offset, sel_y);
}

void Menu_Render(void)
{
    Menu_Render_For(Current_Menu, Sel_Y);
}

void Menu_Enter(void)
{
    Menu_Item_t* item = &Current_Menu->Items[Current_Menu->Cursor];

    if(item->Child_Menu != 0)
    {
        /* 子菜单：从右侧滑入，和整页过渡同一套观感 */
        item->Child_Menu->Cursor = 0;
        item->Child_Menu->Scroll_Offset = 0;
        Menu_Goto_Menu(item->Child_Menu, +1);
    }
    else if(item->Callback != 0)
    {
        item->Callback();
    }
}

/* ------------------------------------------------------------------------- */
/* 按键处理                                                                  */
/* ------------------------------------------------------------------------- */

static void Handle_Key(uint8_t key_pressed)
{
    const SettingId_t* aux_ids;

    if (Current_Page == PAGE_HOME)
    {
        if (key_pressed == KEY_ENTER)
        {
            Current_Menu = &Main_Menu;
            Current_Menu->Cursor = 0;
            Current_Menu->Scroll_Offset = 0;
            Menu_Sel_Reset();
            Menu_Goto(PAGE_MENU, +1);
        }
        return;
    }

    if ((Current_Page == PAGE_INFO) || (Current_Page == PAGE_ATTITUDE))
    {
        if (key_pressed == KEY_BACK)
        {
            Menu_Goto(PAGE_MENU, -1);
        }
        return;
    }

    if ((Current_Page == PAGE_ANGLE_PID) || (Current_Page == PAGE_RATE_PID))
    {
        aux_ids = (Current_Page == PAGE_ANGLE_PID) ? Aux3_Ids : Aux6_Ids;

        switch (key_pressed)
        {
            case KEY_UP:
                if (AUX_Editing)
                {
                    Settings_Adjust(aux_ids[AUX_Cursor], +1);
                }
                else
                {
                    AUX_Cursor = (AUX_Cursor > 0) ? (uint8_t)(AUX_Cursor - 1) : 2;
                }
                break;

            case KEY_UP_HOLD:
                if (AUX_Editing)
                {
                    Settings_Adjust(aux_ids[AUX_Cursor], +10);
                }
                break;

            case KEY_DOWN:
                if (AUX_Editing)
                {
                    Settings_Adjust(aux_ids[AUX_Cursor], -1);
                }
                else
                {
                    AUX_Cursor = (AUX_Cursor < 2) ? (uint8_t)(AUX_Cursor + 1) : 0;
                }
                break;

            case KEY_DOWN_HOLD:
                if (AUX_Editing)
                {
                    Settings_Adjust(aux_ids[AUX_Cursor], -10);
                }
                break;

            case KEY_ENTER:
                AUX_Editing = !AUX_Editing;
                break;

            case KEY_BACK:
                if (AUX_Editing)
                {
                    AUX_Editing = 0;
                }
                else
                {
                    Current_Menu = &PID_Menu;
                    Menu_Sel_Reset();
                    Menu_Goto(PAGE_MENU, -1);
                }
                break;

            default:
                break;
        }
        return;
    }

    if (Current_Page == PAGE_OFFSET)
    {
        switch (key_pressed)
        {
            case KEY_UP:
                if (Offset_Editing)
                {
                    Settings_Adjust(Offset_Ids[Offset_Cursor], +1);
                }
                else
                {
                    Offset_Cursor = (Offset_Cursor > 0) ? (uint8_t)(Offset_Cursor - 1) : 1;
                }
                break;

            case KEY_UP_HOLD:
                if (Offset_Editing)
                {
                    Settings_Adjust(Offset_Ids[Offset_Cursor], +10);
                }
                break;

            case KEY_DOWN:
                if (Offset_Editing)
                {
                    Settings_Adjust(Offset_Ids[Offset_Cursor], -1);
                }
                else
                {
                    Offset_Cursor = (Offset_Cursor < 1) ? (uint8_t)(Offset_Cursor + 1) : 0;
                }
                break;

            case KEY_DOWN_HOLD:
                if (Offset_Editing)
                {
                    Settings_Adjust(Offset_Ids[Offset_Cursor], -10);
                }
                break;

            case KEY_ENTER:
                Offset_Editing = !Offset_Editing;
                break;

            case KEY_BACK:
                if (Offset_Editing)
                {
                    Offset_Editing = 0;
                }
                else
                {
                    Current_Menu = &Main_Menu;
                    Menu_Sel_Reset();
                    Menu_Goto(PAGE_MENU, -1);
                }
                break;

            default:
                break;
        }
        return;
    }

    /* ---- 菜单页 ---- */
    switch (key_pressed)
    {
        case KEY_UP:
            if (Current_Menu->Cursor > 0)
            {
                Current_Menu->Cursor--;
                if (Current_Menu->Cursor < Current_Menu->Scroll_Offset)
                {
                    Current_Menu->Scroll_Offset = Current_Menu->Cursor;
                }
            }
            else
            {
                Current_Menu->Cursor = (uint8_t)(Current_Menu->Item_Count - 1);
                if (Current_Menu->Item_Count > MAX_VISIBLE_ITEMS)
                {
                    Current_Menu->Scroll_Offset = (uint8_t)(Current_Menu->Item_Count - MAX_VISIBLE_ITEMS);
                }
            }
            break;

        case KEY_DOWN:
            if (Current_Menu->Cursor < (uint8_t)(Current_Menu->Item_Count - 1))
            {
                Current_Menu->Cursor++;
                if (Current_Menu->Cursor >= (uint8_t)(Current_Menu->Scroll_Offset + MAX_VISIBLE_ITEMS))
                {
                    Current_Menu->Scroll_Offset = (uint8_t)(Current_Menu->Cursor - MAX_VISIBLE_ITEMS + 1);
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
            Menu_Sel_Reset();
            break;

        case KEY_BACK:
            if (Current_Menu->Parent_Menu == 0)
            {
                Menu_Goto(PAGE_HOME, -1);
            }
            else
            {
                /* 回父菜单：从左侧滑回 */
                Menu_Goto_Menu(Current_Menu->Parent_Menu, -1);
                Menu_Sel_Reset();
            }
            break;

        default:
            break;
    }
}

void Menu_Task_Handler(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t key_pressed = KEY_NONE;

    /* 一屏只取一次快照：保证同一帧数据，渲染过程不持有任何锁 */
    Display_Take_Snapshot();

    /* 过渡动画期间不消费按键，事件留在队列里，动画结束后再处理 */
    if (Nav_Active == 0)
    {
        osMessageQueueGet(KeyEventQueue, &key_pressed, 0, 0);
        Handle_Key(key_pressed);
    }

    Menu_Update_Selection(now);
    Render_Frame();
}
