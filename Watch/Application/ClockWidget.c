//==============================================================================
//  Copyright 2013 Meta Watch Ltd. - http://www.MetaWatch.org/
//
//  Licensed under the Meta Watch License, Version 1.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.MetaWatch.org/licenses/license-1.0.html
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//==============================================================================

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "Messages.h"
#include "MessageQueues.h"

#include "Adc.h"
#include "hal_battery.h"
#include "hal_lpm.h"
#include "hal_miscellaneous.h"
#include "hal_rtc.h"
#include "Wrapper.h"
#include "DebugUart.h"
#include "DrawMsgHandler.h"
#include "SerialRam.h"
#include "Icons.h"
#include "Fonts.h"
#include "LcdDisplay.h"
#include "BitmapData.h"
#include "Property.h"
#include "LcdDriver.h"
#include "LcdBuffer.h"
#include "ClockWidget.h"

#define MAX_DRAW_ITEM_NUM             (12)
/* widget is a list of Draw_t, the order is the WatchFaceId (0 - 14) */
static Draw_t const DrawList[][MAX_DRAW_ITEM_NUM] =
{
  { //1Q
    {DRAW_ID_TYPE_FONT | FUNC_GET_HOUR | Time, 1, 3, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_MIN | Time, 1, 24, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BT_STATE | ICON_SET_BLUETOOTH_BIG, 30, 28, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BATTERY | ICON_SET_BATTERY_V, 35, 3, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DATE | MetaWatch7, 25, 26, DRAW_OPT_OVERLAP_BT | DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_SEC | MetaWatch16, 29, 32, DRAW_OPT_OVERLAP_BT | DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DOW | MetaWatch7, 25, 36, DRAW_OPT_OVERLAP_BT | DRAW_OPT_OVERLAP_SEC | DRAW_OPT_BITWISE_OR}
  },
  { //2Q-TimeG
    {DRAW_ID_TYPE_FONT | FUNC_GET_HOUR | TimeG, 7, 2, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_MIN | TimeG, 53, 2, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BT_STATE | ICON_SET_BLUETOOTH_BIG, 1, 31, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DATE | MetaWatch7, 18, 36, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_SEC | MetaWatch16, 43, 31, /*DRAW_OPT_OVERLAP_BATTERY | */DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DOW | MetaWatch7, 71, 36, /*DRAW_OPT_OVERLAP_BT | DRAW_OPT_OVERLAP_SEC | */DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BATTERY | ICON_SET_BATTERY_H, 70, 35, DRAW_OPT_BITWISE_SET},//38

//    {DRAW_ID_TYPE_BMP | FUNC_GET_BT_STATE | ICON_SET_BLUETOOTH_BIG, 76, 31, DRAW_OPT_BITWISE_OR},
//    {DRAW_ID_TYPE_FONT | FUNC_GET_DATE | MetaWatch7, 7, 35, DRAW_OPT_BITWISE_OR},
//    {DRAW_ID_TYPE_FONT | FUNC_GET_SEC | MetaWatch16, 38, 31, /*DRAW_OPT_OVERLAP_BATTERY | */DRAW_OPT_BITWISE_OR},
////    {DRAW_ID_TYPE_FONT | FUNC_GET_SEC | MetaWatch16, 72, 29, DRAW_OPT_OVERLAP_BT | DRAW_OPT_BITWISE_OR},
//    {DRAW_ID_TYPE_FONT | FUNC_GET_DOW | MetaWatch7, 72, 35, DRAW_OPT_OVERLAP_BT | /*DRAW_OPT_OVERLAP_SEC | */DRAW_OPT_BITWISE_OR},
//    {DRAW_ID_TYPE_BMP | FUNC_GET_BATTERY | ICON_SET_BATTERY_H, 70, 35, DRAW_OPT_BITWISE_SET},//38
  },
  { // 4Q Logo TimeBlock
    {DRAW_ID_TYPE_BMP | FUNC_DRAW_TEMPLATE | TMPL_WGT_LOGO, 0, 0, TMPL_TYPE_4Q, LCD_COL_NUM, LCD_ROW_NUM},
    {DRAW_ID_TYPE_FONT | FUNC_GET_HOUR | TimeBlock, 1, 28, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_MIN | TimeBlock, 51, 28, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_AMPM | MetaWatch5, 80, 50, DRAW_OPT_BITWISE_SET},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BT_STATE | ICON_SET_BLUETOOTH_BIG, 80, 79, DRAW_OPT_BITWISE_SET},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BATTERY | ICON_SET_BATTERY_H, 41, 15, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DATE | MetaWatch16, 2, 12, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_SEC | MetaWatch16, 75, 12, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DOW | MetaWatch16, 68, 12, DRAW_OPT_OVERLAP_SEC | DRAW_OPT_BITWISE_OR}
  },
  { //4Q Big TimeK
    {DRAW_ID_TYPE_BMP | FUNC_DRAW_BLOCK, 0, 0, 0, 12, 17}, // x, w in bytes
    {DRAW_ID_TYPE_BMP | FUNC_DRAW_BLOCK, 0, 79, 0, 12, 17},
    {DRAW_ID_TYPE_FONT | FUNC_GET_HOUR | TimeK, 0, 20, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_MIN | TimeK, 51, 20, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_AMPM | MetaWatch16, 33, 80, DRAW_OPT_BITWISE_NOT},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BT_STATE | ICON_SET_BLUETOOTH_BIG, 76, 1, DRAW_OPT_BITWISE_NOT},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BATTERY | ICON_SET_BATTERY_H, 3, 4, DRAW_OPT_BITWISE_NOT},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DATE | MetaWatch16, 61, 80, DRAW_OPT_BITWISE_NOT},
    {DRAW_ID_TYPE_FONT | FUNC_GET_SEC | MetaWatch16, 39, 1, DRAW_OPT_BITWISE_NOT},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DOW | MetaWatch16, 3, 80, DRAW_OPT_BITWISE_NOT}
  },
  { //4Q-fish
    {DRAW_ID_TYPE_BMP | FUNC_DRAW_TEMPLATE | TMPL_WGT_FISH, 0, 0, TMPL_TYPE_4Q, LCD_COL_NUM, LCD_ROW_NUM},
    {DRAW_ID_TYPE_FONT | FUNC_GET_HOUR | Time, 29, 33, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_MIN | Time, 58, 33, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_AMPM | MetaWatch5, 83, 33, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BT_STATE | ICON_SET_BLUETOOTH_BIG, 82, 2, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_BMP | FUNC_GET_BATTERY | ICON_SET_BATTERY_H, 50, 4, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DATE | MetaWatch7, 55, 22, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_SEC | MetaWatch16, 58, 51, DRAW_OPT_BITWISE_OR},
    {DRAW_ID_TYPE_FONT | FUNC_GET_DOW | MetaWatch7, 58, 55, DRAW_OPT_OVERLAP_SEC}
  },
  { //4Q-Word Clock
    {DRAW_ID_TYPE_BMP | FUNC_DRAW_TEMPLATE | TMPL_WGT_WORD, 0, 0, TMPL_TYPE_4Q, LCD_COL_NUM, LCD_ROW_NUM},
    {DRAW_ID_TYPE_BMP | FUNC_DRAW_WORDCLK | Time, 0, 0, DRAW_OPT_BITWISE_DST_NOT}
  },
};

typedef struct
{
  unsigned char LayoutType;
  unsigned char ItemNum;
} Widget_t;

//
static Widget_t const ClockWidget[] =
{
  {LAYOUT_QUAD_SCREEN, 7}, // 1Q
  {LAYOUT_HORI_SCREEN, 7}, // 2Q
  {LAYOUT_FULL_SCREEN, 9}, // logo
  {LAYOUT_FULL_SCREEN, 10}, // big
  {LAYOUT_FULL_SCREEN, 9}, // fish
  {LAYOUT_FULL_SCREEN, 2}, // hanzi
};

#define HOME_WIDGET_NUM (sizeof(ClockWidget) / sizeof(Widget_t))

#define WORD_CLK_WIDTH     15
#define WORD_CLK_WIDTH_IN_BYTES  ((WORD_CLK_WIDTH >> 3) + 1)
#define WORD_CLK_HEIGHT    18
#define WORD_CLK_PER_LINE  6

void MarkChar(unsigned char Index, Draw_t *Info);
void MarkWord(unsigned char Index,unsigned char Size, Draw_t *Info);

typedef struct
{
	unsigned char index;
	unsigned char size;
} WordPos_t;

static WordPos_t const ClockWord_H[] =
{
	{40,6},
	{50,3},
	{27,3},
	{5,5},
	{0,4},
	{23,4},
	{10,3},
	{14,5},
	{45,5},
	{35,4},
	{20,3},
	{30,6}
};

static WordPos_t const ClockWord_MM[] =
{
	/*MINUTE*/
	{73,1},
	{96,4},
	{70,3},
	{60,7},
	{90,6},
	{90,6},
	{54,6},
	{54,6},
	{85,5},
	{85,5},
	{80,5},
	{80,5}
};

static WordPos_t const ClockWord_SM[] =
{
	/*SUB MINUTE*/
	{75,5},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{96,4},
	{0,0},
	{96,4},
	{0,0},
	{96,4},
	{0,0},
	{96,4}
};

static WordPos_t const ClockWord_AM = {100,2};
static WordPos_t const ClockWord_PM = {102,2};
static WordPos_t const ClockWord_BATT = {106,4};

/******************************************************************************/

void DrawClockWidget(unsigned char ClockId)
{
  unsigned char *pDrawBuffer = (unsigned char *)GetDrawBuffer();

  unsigned char FaceId = FACE_ID(ClockId);
  unsigned int BufSize = Layout[ClockWidget[FaceId].LayoutType].QuadNum * BYTES_PER_QUAD + SRAM_HEADER_LEN;
  memset(pDrawBuffer, 0, BufSize);

  Draw_t Info;
  unsigned int i;

  for (i = 0; i < ClockWidget[FaceId].ItemNum; ++i)
  {
    memcpy((unsigned char *)&Info, (unsigned char *)&DrawList[FaceId][i], sizeof(Draw_t));
    Info.Opt |= FaceId << 4; //ClockId & 0xF0; // FaceId
    Draw(&Info, NULL, IDLE_MODE);
  }

  WriteClockWidget(FaceId, pDrawBuffer);
}

void DrawBitmapToIdle(Draw_t *Info, unsigned char WidthInBytes, unsigned char const *pBitmap)
{
  unsigned char *pByte = Info->Y / HALF_SCREEN_ROWS * BYTES_PER_QUAD * 2 +
                         Info->Y % HALF_SCREEN_ROWS * BYTES_PER_QUAD_LINE +
                         Info->X / HALF_SCREEN_COLS * BYTES_PER_QUAD +
                         (Info->X % HALF_SCREEN_COLS >> 3) +
                         (unsigned char *)GetDrawBuffer() + SRAM_HEADER_LEN;

  unsigned char ColBit = 1 << Info->X % 8; // dst
  unsigned char MaskBit = BIT0; // src
  unsigned int Delta;
  unsigned char Set; // src bit is set or clear
  unsigned char x, y;

  for (x = 0; x < Info->Width; ++x)
  {
    for (y = 0; y < Info->Height; ++y)
    {
      Delta = (ClockWidget[FACE_ID(Info->Opt)].LayoutType == LAYOUT_FULL_SCREEN) &&
              (Info->Y < HALF_SCREEN_ROWS && (Info->Y + y) >= HALF_SCREEN_ROWS) ?
              BYTES_PER_QUAD : 0;
      
      if(pBitmap==NULL)
      {
    	  unsigned char *pTarget = (pByte + y * BYTES_PER_QUAD_LINE + Delta);
    	  *pTarget |= ColBit;
      }
      else
      {
    	  Set = *(pBitmap + y * WidthInBytes) & MaskBit;
    	  BitOp(pByte + y * BYTES_PER_QUAD_LINE + Delta, ColBit, Set, Info->Opt & DRAW_OPT_BITWISE_MASK);
      }
    }

    MaskBit <<= 1;
    if (MaskBit == 0)
    {
      MaskBit = BIT0;

      if(pBitmap!=NULL)
      	  pBitmap ++;
    }
    
    ColBit <<= 1;
    if (ColBit == 0)
    {
      ColBit = BIT0;
      pByte ++;
      // check next pixel x
      if ((Info->X + x + 1) == HALF_SCREEN_COLS) pByte += BYTES_PER_QUAD - BYTES_PER_QUAD_LINE;
    }
  }
}

void DrawTemplateToIdle(Draw_t *Info)
{
#if __IAR_SYSTEMS_ICC__
  const unsigned char __data20 *pTemp;
#else
  const unsigned char *pTemp;
#endif

  unsigned char *pByte = (unsigned char *)GetDrawBuffer() + SRAM_HEADER_LEN;
  unsigned char TempId = Info->Id & TMPL_ID_MASK;

  pTemp =  ((Info->Opt & 0x0F) == TMPL_TYPE_4Q) ? pTemplate[TempId] : pTemplate2Q[TempId];
  unsigned char RowNum = ((Info->Opt & 0x0F) == TMPL_TYPE_4Q) ? LCD_ROW_NUM : HALF_SCREEN_ROWS;

//  PrintF("DrwTmpl Id:%d, Row:%d", TempId, RowNum);

  unsigned char i, k;

  for (i = 0; i < RowNum; ++i)
  {
    if (i == HALF_SCREEN_ROWS) pByte += BYTES_PER_QUAD;

    for (k = 0; k < BYTES_PER_QUAD_LINE; ++k) pByte[k] = pTemp[k];
    for (k = 0; k < BYTES_PER_QUAD_LINE; ++k) pByte[k + BYTES_PER_QUAD] = pTemp[k + BYTES_PER_QUAD_LINE];

    pByte += BYTES_PER_QUAD_LINE;
    pTemp += BYTES_PER_LINE;
  }
}

void DrawBlockToIdle(Draw_t *Info)
{
  unsigned char *pByte = (unsigned char *)GetDrawBuffer() + SRAM_HEADER_LEN + Info->X + Info->Y * BYTES_PER_QUAD_LINE;
  if (Info->Y > HALF_SCREEN_ROWS) pByte += BYTES_PER_QUAD;

  unsigned char i;

  for (i = 0; i < Info->Height; ++i)
  {
    if (Info->Y + i == HALF_SCREEN_ROWS) pByte += BYTES_PER_QUAD;

    memset(pByte, 0xFF, BYTES_PER_QUAD_LINE);
    memset(pByte + BYTES_PER_QUAD, 0xFF, BYTES_PER_QUAD_LINE);
    pByte += BYTES_PER_QUAD_LINE;
  }
}

void DrawWordClock(Draw_t *Info)
{
	Info->Id = DRAW_ID_TYPE_BMP;
	Info->Opt |= DRAW_OPT_BITWISE_NOT;

	//Hour
	unsigned char Time = To12H(RTCHOUR);
	MarkWord(ClockWord_H[Time].index,ClockWord_H[Time].size,Info);

	//Minute
	Time = (RTCMIN / 5);
	MarkWord(ClockWord_MM[Time].index,ClockWord_MM[Time].size,Info);
	MarkWord(ClockWord_SM[Time].index,ClockWord_SM[Time].size,Info);

	//AM:PM
	if(RTCHOUR > 0x11)
		MarkWord(ClockWord_PM.index,ClockWord_PM.size,Info); //PM
	else
		MarkWord(ClockWord_AM.index,ClockWord_AM.size,Info); //AM

	//Battery status
	if(BatteryPercentage() <= WARNING_LEVEL)
		MarkWord(ClockWord_BATT.index,ClockWord_BATT.size,Info);
}

void MarkWord(unsigned char Index, unsigned char Size, Draw_t *Info)
{
	if(Size==0)return;

	Info->X = 3 + ( Index % WORD_CLK_PER_LINE * (WORD_CLK_WIDTH));
	Info->Y = 4 + ( Index / WORD_CLK_PER_LINE * (WORD_CLK_HEIGHT+1));
	Info->Height = WORD_CLK_HEIGHT;
	Info->Width = WORD_CLK_WIDTH*Size;
	DrawBitmapToIdle(Info, WIDTH_IN_BYTES(Info->Width), (unsigned char *)GetDrawBuffer());

	Info->Height = 1;
	Info->Y = Info->Y - 1;
	DrawBitmapToIdle(Info, WIDTH_IN_BYTES(Info->Width), NULL); // Add top border
	Info->Y = Info->Y + WORD_CLK_HEIGHT + 1;
	DrawBitmapToIdle(Info, WIDTH_IN_BYTES(Info->Width), NULL); // Add bottom border
}
