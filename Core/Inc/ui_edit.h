#ifndef __UI_EDIT_H__
#define __UI_EDIT_H__

#include "main.h"

/* 一个可编辑字段：标签 + 当前值（1000~2000） */
typedef struct
{
    const char* label;
    uint16_t    value;
} UiField_t;

/*
 * 数值编辑页：顶部标签条 + 中间大号数值 + 底部进度条。
 *   cursor  —— 当前字段
 *   editing —— 是否处于编辑状态
 */
void UI_Edit_Render(const char* title, const UiField_t* fields, uint8_t count,
                    uint8_t cursor, uint8_t editing);

#endif
