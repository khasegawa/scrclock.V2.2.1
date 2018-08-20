#include "stdafx.h"

static WCHAR week[][2] = {L"“ú", L"ŒŽ", L"‰Î", L"…", L"–Ø", L"‹à", L"“y"};
static WCHAR dateStr[] = L"12.31 “ú ";
static WCHAR timeStr[] = L"10:58:00";

void setTimeString(bool showdate, bool showsec)
{
	SYSTEMTIME time;
	GetLocalTime(&time);
	if (showdate) {
		wsprintf(dateStr, L"%d.%d %s ", time.wMonth, time.wDay, week[time.wDayOfWeek]);
	} else {
		*dateStr = 0;
	}
	if (showsec) {
		wsprintf(timeStr, L"%02d:%02d:%02d", time.wHour, time.wMinute, time.wSecond);
	} else {
		wsprintf(timeStr, L"%02d:%02d", time.wHour, time.wMinute);
	}
}

SIZE drawTimeString(HDC hdc, HWND hwnd, int lines, BOOL doDraw)
{
	SIZE dateSize, timeSize;
	GetTextExtentPoint32(hdc, dateStr, wcslen(dateStr), &dateSize);
	GetTextExtentPoint32(hdc, timeStr, wcslen(timeStr), &timeSize);

	SIZE winSize;
	int x, y;
	if (lines == 1) {
		winSize.cx = dateSize.cx + timeSize.cx;
		winSize.cy = dateSize.cy > timeSize.cy ? dateSize.cy : timeSize.cy;
		x = dateSize.cx;
		y = 0;
	} else {
		winSize.cx = dateSize.cx > timeSize.cx ? dateSize.cx : timeSize.cx;
		winSize.cy = dateSize.cy + timeSize.cy;
		x = 0;
		y = dateSize.cy;
	}

	if (doDraw) {
		TextOut(hdc, 0, 0, dateStr, wcslen(dateStr));
		TextOut(hdc, x, y, timeStr, wcslen(timeStr));
	}

	return winSize;
}
