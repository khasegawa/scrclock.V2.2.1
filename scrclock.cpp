// scrclock.cpp : アプリケーションのエントリ ポイントを定義します。
//
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

#include "stdafx.h"
#include <stdio.h>
#include <commctrl.h>
#include <crtdbg.h>
#include <Commdlg.h>
#include "resource.h"

#define WM_TRAYICONMESSAGE WM_USER

#define ID_TIMER  1
#define IDM_EXIT  105

#define MAX_LOADSTRING 100

#define INTERVAL         1000                // 表示間隔 (ms)
#define UIFONT           L"HGS創英角ｺﾞｼｯｸUB" // 文字フォント
#define FONTSIZE         50                  // 文字の大きさ
#define TRANSPARENTALPHA 90                  // 文字塗りつぶし透明度 (X/255)
#define FILLCOLOR        RGB(0, 0, 0)        // 文字塗りつぶし色
#define PENCOLOR         RGB(200, 200, 200)  // 文字枠色

typedef enum { TL, TC, TR, L, C, R, BL, BC, BR } POS;
typedef struct {
	POS pos;
	int lines;
	bool showdate : 1;
	bool showsec : 1;
	bool showfrm : 1;
	int fontsize;
	COLORREF fillcolor;
	COLORREF pencolor;
	int alpha;
} PROP;

static PROP prop = { TC, 1, 1, 0, 1, FONTSIZE, FILLCOLOR, PENCOLOR, TRANSPARENTALPHA };

static HINSTANCE hInst;                // 現在のインターフェイス
static TCHAR szTitle[MAX_LOADSTRING];  // タイトル バーのテキスト
static WCHAR *prgPath;                 // このプログラムファイルのフルパス
static HMENU hMenu;
static HFONT hFont;
static HBRUSH hBrush;
static HPEN hPen;
static bool propDlgExist = false;

static ATOM MyRegisterClass(HINSTANCE, TCHAR [], WNDPROC, HBRUSH);
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK WndProcSub(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK SetProp(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
static void CALLBACK TimerFunc(HWND, UINT, UINT, DWORD);
static void setWinPos(HDC hdc, HWND hwnd, int opt);
static void setWindowRightBottom(HWND);
static void saveProps();
static void loadProps();

extern void setTimeString(bool, bool);
extern SIZE drawTimeString(HDC, HWND, int, BOOL);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

    _CrtSetReportMode(_CRT_ASSERT, 0);

	prgPath = __wargv[0];

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

	// プロパティを保存済み設定ファイルから読み込む
	loadProps();

	TCHAR szWindowClass[MAX_LOADSTRING];  // メイン ウィンドウ クラス名
	hBrush = CreateSolidBrush(prop.fillcolor);
	MyRegisterClass(hInstance, szWindowClass, WndProc, hBrush);
	MyRegisterClass(hInstance, L"Sub", WndProcSub, (HBRUSH)GetStockObject(BLACK_BRUSH));

	// 動かすインスタンスは一つだけ
	HWND hWnd = FindWindow(szWindowClass, NULL);
	if (hWnd) {
		MessageBox(NULL, L"このプログラムは既に動いています。\nタスクトレイを確認してください。", szTitle, MB_OK);
		SetForegroundWindow(hWnd);
		return 0;
	}

	hInst = hInstance; // グローバル変数にインスタンス処理を格納します。
	
	hFont = CreateFont(prop.fontsize, 0, 0, 0, FW_HEAVY, 0, 0, 0,
		SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, UIFONT);

	// 文字列の「塗りつぶし表示」用のウィンドウ
	hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		szWindowClass, L"", WS_POPUP,
		0, 0, 1, 1, // 注：幅、高さを0にすると、後から文字列の大きさに変えようとしても変えられない
		NULL, NULL, hInstance, NULL);
	if (! hWnd) {
		return 0;
	}
	SetLayeredWindowAttributes(hWnd, 0, prop.alpha, LWA_ALPHA);
	ShowWindow(hWnd, nCmdShow);

	// 文字列の「枠だけ表示」用のウィンドウ
	HWND hWndSub = CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_COMPOSITED,
		L"Sub", L"", WS_POPUP,
		0, 0, 1, 1, // 注：幅、高さを0にすると、後から文字列の大きさに変えようとしても変えられない
		hWnd, NULL, hInstance, NULL);
	if (! hWndSub) {
		return 0;
	}
	SetLayeredWindowAttributes(hWndSub, RGB(0,0,0), 0, LWA_COLORKEY);  // "Sub"ウィンドウクラスの背景色を透過色とする
	ShowWindow(hWndSub, nCmdShow);

	// タイマー起動
	if (SetTimer(hWndSub, (UINT)ID_TIMER, (UINT)INTERVAL, (TIMERPROC)TimerFunc) == 0) {
		MessageBox(hWnd, L"タイマー起動のエラーです。終了します。", L"エラー", MB_OK);
		DestroyWindow(hWnd);
	}

	// メイン メッセージ ループ:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
//  コメント:
//    この関数および使い方は、'RegisterClassEx' 関数が追加された
//    Windows 95 より前の Win32 システムと互換させる場合にのみ必要です。
//    アプリケーションが、関連付けられた
//    正しい形式の小さいアイコンを取得できるようにするには、
//    この関数を呼び出してください。
//
static ATOM MyRegisterClass(HINSTANCE hInstance, TCHAR *className, WNDPROC wndProc, HBRUSH hBrush)
{
	WNDCLASSEX wcex;

	wcex.cbSize        = sizeof(WNDCLASSEX);
	wcex.style         = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc   = wndProc;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hInstance     = hInstance;
	wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LARGE));
	wcex.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = hBrush;
	wcex.lpszMenuName  = NULL;
	wcex.lpszClassName = className;

	return RegisterClassEx(&wcex);
}

// タスクトレイ上のアイコンの右クリックメニューを作成する
static void createMenu(HWND hwnd, HINSTANCE hInstance, TCHAR *title, HMENU *phMenu, NOTIFYICONDATA *pnid)
{
	*phMenu = CreatePopupMenu();

	MENUITEMINFO menuiteminfo[4], *mp = menuiteminfo;
	mp->cbSize = sizeof(*mp);
	mp->fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID;
	mp->fType = MFT_STRING;
	mp->fState = MFS_DEFAULT | MFS_HILITE;
	mp->wID = IDD_PROPBOX;
	mp->dwTypeData = L"設定...";
	mp->cch = (unsigned int)wcslen(mp->dwTypeData);
	InsertMenuItem(*phMenu, 0, true, mp);

	mp++;
	mp->cbSize = sizeof(*mp);
	mp->fMask = MIIM_TYPE | MIIM_ID;
	mp->fType = MFT_STRING;
	mp->wID = IDD_ABOUTBOX;
	mp->dwTypeData = L"このソフトウェアについて...";
	mp->cch = wcslen(mp->dwTypeData);
	InsertMenuItem(*phMenu, 1, true, mp);

	mp++;
	mp->cbSize = sizeof(*mp);
	mp->fMask = MIIM_TYPE;
	mp->fType = MFT_SEPARATOR;
	InsertMenuItem(*phMenu, 2, true, mp);

	mp++;
	mp->cbSize = sizeof(*mp);
	mp->fMask = MIIM_TYPE | MIIM_ID;
	mp->fType = MFT_STRING;
	mp->wID = IDM_EXIT;
	mp->dwTypeData = L"終了";
	mp->cch = wcslen(mp->dwTypeData);
	InsertMenuItem(*phMenu, 3, true, mp);

	pnid->cbSize = sizeof(NOTIFYICONDATA);
	pnid->hWnd = hwnd;
	pnid->uID = 0;
	pnid->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	pnid->uCallbackMessage = WM_TRAYICONMESSAGE;
	pnid->hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcsncpy_s(pnid->szTip, title, _TRUNCATE);
	Shell_NotifyIcon(NIM_ADD, pnid);
}

static void _showDialog(HWND hWnd, HMENU hMenu, int menuid, int resid, INT_PTR CALLBACK func(HWND, UINT, WPARAM, LPARAM))
{
	EnableMenuItem(hMenu, menuid, MF_BYPOSITION | MF_GRAYED);  // 同じダイアログメニューを重ねて呼べないようにする
	DialogBox(hInst, MAKEINTRESOURCE(resid), hWnd, func);
	EnableMenuItem(hMenu, menuid, MF_BYPOSITION | MF_ENABLED); // 同じダイアログメニューを元に戻す
}

static void ShowSetPropDialog(HWND hWnd)
{
	if (!propDlgExist) {
		propDlgExist = true;
		_showDialog(hWnd, hMenu, 0, IDD_PROPBOX, SetProp);
		propDlgExist = false;
	}
}

static void ShowAboutDialog(HWND hWnd)
{
	static bool exist = false;
	if (!exist) {
		exist = true;
		_showDialog(hWnd, hMenu, 1, IDD_ABOUTBOX, About);
		exist = false;
	}
}

//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的:  メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND	- アプリケーション メニューの処理
//  WM_PAINT	- メイン ウィンドウの描画
//  WM_DESTROY	- 中止メッセージを表示して戻る
//
//
static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static NOTIFYICONDATA nid;

	switch (message) {
	case WM_CREATE:
		{
			INITCOMMONCONTROLSEX ic;  // INITCOMMONCONTROLSEX構造体
			//コモンコントロールの初期化  
			ic.dwICC = ICC_COOL_CLASSES;
			ic.dwSize = sizeof(INITCOMMONCONTROLSEX);
			InitCommonControlsEx(&ic);

			createMenu(hWnd, hInst, szTitle, &hMenu, &nid);
			setTimeString(prop.showdate, prop.showsec);
		}
		break;

	case WM_COMMAND:
		{
			int wmId    = LOWORD(wParam);
			int wmEvent = HIWORD(wParam);
			// 選択されたメニューの解析:
			switch (wmId) {
			case IDD_PROPBOX:
				ShowSetPropDialog(hWnd);
				break;
			case IDD_ABOUTBOX:
				ShowAboutDialog(hWnd);
				break;
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;

	case WM_TRAYICONMESSAGE:
		switch (lParam) {
		case WM_LBUTTONDOWN:
			ShowSetPropDialog(hWnd);
			break;
		case WM_RBUTTONDOWN: // タスクトレイアイコン上で右クリック
			POINT pt;
			GetCursorPos(&pt);
			SetForegroundWindow(hWnd);  // これがないと、ポップアップメニューが消えなくなる
			TrackPopupMenuEx(hMenu, TPM_LEFTALIGN, pt.x, pt.y, hWnd, 0);
			break;
		}
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			// TODO: 描画コードをここに追加してください...

			SetBkMode(hdc, TRANSPARENT);
			SelectObject(hdc, hFont);

			setWinPos(hdc, hWnd, propDlgExist ? SWP_NOZORDER : 0);

			BeginPath(hdc);
			(void)drawTimeString(hdc, hWnd, prop.lines, true);
			EndPath(hdc);

			HRGN hrgn = PathToRegion(hdc);
			SetWindowRgn(hWnd, hrgn, FALSE);

			EndPaint(hWnd, &ps);
		}
		break;

	case WM_DESTROY:
		DestroyMenu(hMenu);
		Shell_NotifyIcon(NIM_DELETE, &nid);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

static LRESULT CALLBACK WndProcSub(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_CREATE:
		hPen = CreatePen(PS_SOLID, 1, prop.pencolor);
		break;

	case WM_PAINT:
		{
			// メインウィンドウの描画を強制
			RECT rect;
			GetClientRect(GetParent(hWnd), &rect);
			InvalidateRect(GetParent(hWnd), &rect, TRUE);

			// Subウィンドウの描画
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			// TODO: 描画コードをここに追加してください...

			if (prop.showfrm) {
				SetBkMode(hdc, TRANSPARENT);
				SelectObject(hdc, hFont);
				SelectObject(hdc, hPen);

				setWinPos(hdc, hWnd, SWP_NOZORDER);

				BeginPath(hdc);
				(void)drawTimeString(hdc, hWnd, prop.lines, true);
				EndPath(hdc);

				StrokePath(hdc);
			}
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

UINT CALLBACK CCHookProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
		case WM_INITDIALOG:
			SetWindowText(hdlg, L"文字色設定");
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

static void SetDispColor(HWND hWnd, int id)
{
	//カスタムカラーを配列に格納(16色まで設定可能)
	static COLORREF CustColors2[16] = {
		RGB(255, 255, 255), RGB(0, 0, 0), RGB(255, 0, 0), RGB(0, 255, 0),
		RGB(0, 0, 255), RGB(255, 255, 0), RGB(0, 255, 255), RGB(255, 0, 255)
	};

	//CHOOSECOLOR構造体
	CHOOSECOLOR cc;
	cc.lStructSize = sizeof cc;
	cc.hwndOwner = hWnd;
	cc.hInstance = NULL;
	cc.rgbResult = (id == IDC_FRMCOLOR) ? prop.pencolor : prop.fillcolor;
	cc.lpCustColors = CustColors2;
	cc.Flags = CC_ENABLEHOOK | CC_FULLOPEN | CC_RGBINIT;
	cc.lCustData = NULL;
	cc.lpfnHook = CCHookProc;
	cc.lpTemplateName = NULL;

	//カラーダイアログ表示
	
	ChooseColor(&cc);
	if (id == IDC_FRMCOLOR) {
		DeleteObject(hPen);
		prop.pencolor = cc.rgbResult;
		if (prop.pencolor == RGB(0, 0, 0)) {
			prop.pencolor = RGB(1, 0, 0);  // 黒は透過色に設定されているので、ちょっと変える
		}
		hPen = CreatePen(PS_SOLID, 1, prop.pencolor);
	} else {
		DeleteObject(hBrush);
		prop.fillcolor = cc.rgbResult;
		hBrush = CreateSolidBrush(prop.fillcolor);
		SetClassLong(GetParent(hWnd), GCL_HBRBACKGROUND, (LONG)hBrush);
	}
}

static BOOL DialogCommand(HWND hWnd, int id)
{
	switch (id) {
	case IDC_POS_TL:
		prop.pos = TL;
		break;
	case IDC_POS_TC:
		prop.pos = TC;
		break;
	case IDC_POS_TR:
		prop.pos = TR;
		break;
	case IDC_POS_L:
		prop.pos = L;
		break;
	case IDC_POS_C:
		prop.pos = C;
		break;
	case IDC_POS_R:
		prop.pos = R;
		break;
	case IDC_POS_BL:
		prop.pos = BL;
		break;
	case IDC_POS_BC:
		prop.pos = BC;
		break;
	case IDC_POS_BR:
		prop.pos = BR;
		break;
	case IDC_DISP_ONELINE:
		prop.lines = 1;
		break;
	case IDC_DISP_TWOLINES:
		prop.lines = 2;
		break;
	case IDC_SHOW_DATE:
		prop.showdate = (IsDlgButtonChecked(hWnd, IDC_SHOW_DATE) == BST_CHECKED);
		EnableWindow(GetDlgItem(hWnd, IDC_DISP_ONELINE), prop.showdate);
		EnableWindow(GetDlgItem(hWnd, IDC_DISP_TWOLINES), prop.showdate);
		break;
	case IDC_SHOW_SEC:
		prop.showsec = (IsDlgButtonChecked(hWnd, IDC_SHOW_SEC) == BST_CHECKED);
		break;
	case IDC_FRMDISP:
		prop.showfrm = (IsDlgButtonChecked(hWnd, IDC_FRMDISP) == BST_CHECKED);
		SetWindowText(GetDlgItem(hWnd, IDC_FRMDISP), prop.showfrm ? L"On" : L"Off");
		
		EnableWindow(GetDlgItem(hWnd, IDC_FRMCOLOR), prop.showfrm);
		break;
	case IDC_FONTSIZE:
		DeleteObject(hFont);
		WCHAR buf[5];
		GetWindowText(GetDlgItem(hWnd, IDC_FONTSIZE), buf, sizeof buf);
		prop.fontsize = _wtoi(buf);
		hFont = CreateFont(prop.fontsize, 0, 0, 0, FW_HEAVY, 0, 0, 0,
			SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, UIFONT);
		break;
	case IDC_FRMCOLOR:
	case IDC_FILLCOLOR:
		SetDispColor(hWnd, id);
		break;
	case IDOK:
		saveProps();
		EndDialog(hWnd, id);
		break;
	case IDCANCEL:
		EndDialog(hWnd, id);
		break;
	default:
		return (INT_PTR)FALSE;
	}

	return (INT_PTR)TRUE;
}

INT_PTR CALLBACK SetProp(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	switch (message) {
	case WM_INITDIALOG:
		{
			// 表示位置
			int idChecked =
				(prop.pos == TL) ? IDC_POS_TL :
				(prop.pos == TC) ? IDC_POS_TC :
				(prop.pos == TR) ? IDC_POS_TR :
				(prop.pos == L)  ? IDC_POS_L  :
				(prop.pos == C)  ? IDC_POS_C  :
				(prop.pos == R)  ? IDC_POS_R  :
				(prop.pos == BL) ? IDC_POS_BL :
				(prop.pos == BC) ? IDC_POS_BC : IDC_POS_BR;
			SendMessage(GetDlgItem(hwndDlg, idChecked), BM_SETCHECK, BST_CHECKED, 0);

			// 表示項目
			if (prop.showdate) {
				SendMessage(GetDlgItem(hwndDlg, IDC_SHOW_DATE), BM_SETCHECK, BST_CHECKED, 0);
			}
			if (prop.showsec) {
				SendMessage(GetDlgItem(hwndDlg, IDC_SHOW_SEC), BM_SETCHECK, BST_CHECKED, 0);
			}

			// 表示行数
			HWND hWndOne = GetDlgItem(hwndDlg, IDC_DISP_ONELINE);
			HWND hWndTwo = GetDlgItem(hwndDlg, IDC_DISP_TWOLINES);
			if (prop.lines == 1) {
				SendMessage(hWndOne, BM_SETCHECK, BST_CHECKED, 0);
			} else {
				SendMessage(hWndTwo, BM_SETCHECK, BST_CHECKED, 0);
			}
			prop.showdate = (IsDlgButtonChecked(hwndDlg, IDC_SHOW_DATE) == BST_CHECKED);
			EnableWindow(hWndOne, prop.showdate);
			EnableWindow(hWndTwo, prop.showdate);

			// 文字の大きさ
			HFONT hCbFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
				SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY, DEFAULT_PITCH, L"MS UI Gothic");
			HWND hWndCb = GetDlgItem(hwndDlg, IDC_FONTSIZE);
			SendMessage(hWndCb, WM_SETFONT, (WPARAM)hCbFont, 0);
			WCHAR buf[][5] = { L"20", L"24", L"28", L"35", L"42", L"50", L"60", L"72", L"86", L"100", L"120", L"150",
								L"180", L"210", L"260", L"310", L"370", L"440", L"530", L"640", L"770", L"920"};
			for (int i = 0; i < sizeof buf / sizeof *buf; i++) {
				(void)SendMessage(hWndCb, CB_ADDSTRING, 0, (LPARAM)buf[i]);
				if (_wtoi(buf[i]) == prop.fontsize) {
					SendMessage(hWndCb, CB_SETCURSEL, i, 0);
				}
			}

			// 文字枠表示
			SendMessage(GetDlgItem(hwndDlg, IDC_FRMDISP), BM_SETCHECK, prop.showfrm ? BST_CHECKED : BST_UNCHECKED, 0);
			SetWindowText(GetDlgItem(hwndDlg, IDC_FRMDISP), prop.showfrm ? L"On" : L"Off");
			EnableWindow(GetDlgItem(hwndDlg, IDC_FRMCOLOR), prop.showfrm);

			// 透明度
			SendMessage(GetDlgItem(hwndDlg, IDC_SLIDER1), TBM_SETRANGE, 0, (LPARAM)MAKELONG(0, 100));
			int value = 100 - (prop.alpha * 100 / 255);
			SendMessage(GetDlgItem(hwndDlg, IDC_SLIDER1), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)value);

			setWindowRightBottom(hwndDlg);
		}
		return (INT_PTR)TRUE;

	case WM_HSCROLL:
		if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_SLIDER1)) {
			// スライダーの値を取得する
			int value = (int)SendMessage(GetDlgItem(hwndDlg, IDC_SLIDER1), TBM_GETPOS, 0, 0);
			prop.alpha = 255 - (value * 255 / 100);
			SetLayeredWindowAttributes(GetParent(hwndDlg), 0, prop.alpha, LWA_ALPHA);
			return (INT_PTR)TRUE;
		}
		break;

	case WM_COMMAND:
		return DialogCommand(hwndDlg, LOWORD(wParam));

	}
	return (INT_PTR)FALSE;
}

// バージョン情報ボックスのメッセージ ハンドラです。
static INT_PTR CALLBACK About(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message) {
	case WM_INITDIALOG:
		{
			// ダイアログをスクリーン右下部に表示する
			setWindowRightBottom(hwndDlg);

			int bufSize = GetFileVersionInfoSize(prgPath, 0);
			if (bufSize > 0) {
				void *buf = (void *)malloc(sizeof (BYTE) * bufSize);
				if (buf != NULL) {
					GetFileVersionInfo(prgPath, 0, bufSize, buf);

					void *str;
					unsigned int strSize;

					VerQueryValue(buf, L"\\StringFileInfo\\0411fde9\\ProductName", (LPVOID *)&str, &strSize);
					SetWindowText(hwndDlg, (WCHAR *)str);
					SetWindowText(GetDlgItem(hwndDlg, IDC_EDIT2), (WCHAR *)str);

					VerQueryValue(buf, L"\\StringFileInfo\\0411fde9\\FileVersion", (LPVOID *)&str, &strSize);
					SetWindowText(GetDlgItem(hwndDlg, IDC_EDIT1), (WCHAR *)str);

					VerQueryValue(buf, L"\\StringFileInfo\\0411fde9\\LegalCopyright", (LPVOID *)&str, &strSize);
					SetWindowText(GetDlgItem(hwndDlg, IDC_EDIT3), (WCHAR *)str);

					free(buf);
				}
			}
		}
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hwndDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

static void CALLBACK TimerFunc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	setTimeString(prop.showdate, prop.showsec);

	// Subウィンドウの描画を強制
	RECT rect;
	GetClientRect(hwnd, &rect);
	InvalidateRect(hwnd, &rect, TRUE);
}

#if 0
static SIZE getScreenSize()
{
	SIZE scrSize;

	// 現在の解像度を得る
	DEVMODE devMode;
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
		scrSize.cx = devMode.dmPelsWidth;
		scrSize.cy = devMode.dmPelsHeight;
	} else {
		// 万一解像度が取得できない場合の適当な値
		scrSize.cx = 640;
		scrSize.cy = 480;
	}
	return scrSize;
}
#endif

static RECT getScreenRect()
{
	// 現在のスクリーンワーキングエリアのサイズを得る
	POINT point = { 0, 0 };
	MONITORINFOEX monitorInfo;
	monitorInfo.cbSize = sizeof monitorInfo;
	HMONITOR hMonitor = MonitorFromPoint(point, MONITOR_DEFAULTTOPRIMARY);
	GetMonitorInfo(hMonitor, &monitorInfo);

	return monitorInfo.rcWork;
}

static void setWinPos(HDC hdc, HWND hwnd, int opt)
{
	SIZE winSize = drawTimeString(hdc, hwnd, prop.lines, false);
	RECT scrRect = getScreenRect();
	POINT p;

	p.x =
		(prop.pos == TL || prop.pos == L || prop.pos == BL) ?
			scrRect.left : // 左
		(prop.pos == TR || prop.pos == R || prop.pos == BR) ?
			scrRect.right - winSize.cx : // 右
			scrRect.left + (scrRect.right - scrRect.left - winSize.cx) / 2; // 左右中央

	p.y =
		(prop.pos == TL || prop.pos == TC || prop.pos == TR) ?
			scrRect.top : // 上
		(prop.pos == BL || prop.pos == BC || prop.pos == BR) ?
			scrRect.bottom - winSize.cy : // 下
			scrRect.top + (scrRect.bottom - scrRect.top - winSize.cy) / 2; // 上下中央

	SetWindowPos(hwnd, 0, p.x, p.y, winSize.cx, winSize.cy, SWP_NOACTIVATE | opt);
}

// ウィンドウをスクリーン右下部に表示する
static void setWindowRightBottom(HWND hwnd)
{
	RECT scrRect = getScreenRect();
	RECT rect;
	GetWindowRect(hwnd, &rect);
	int x = scrRect.right - (rect.right - rect.left) - 20;
	int y = scrRect.bottom - (rect.bottom - rect.top) - 20;
	SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
}

// プロパティを保存する
static void saveProps()
{
	int ccount = wcslen(prgPath) + 4 + 1;
	WCHAR *fname = (WCHAR *)malloc(sizeof(WCHAR) * ccount);
	if (fname == NULL) {
		return;
	}
	wcscpy_s(fname, ccount, prgPath);

	WCHAR *p = wcsrchr(fname, L'.');
	if (p == NULL) {
		wcscat_s(fname, ccount, L".ini");
	} else {
		wcscpy_s(p, ccount - (p - fname), L".ini");
	}

	FILE *fp;
	if (_wfopen_s(&fp, fname, L"w") != 0) {
		free(fname);
		return;
	}
	free(fname);
	fwprintf(fp, L"%d\n%d\n%d\n%d\n%d\n%d\n%lu\n%lu\n%d\n",
		prop.pos, prop.lines, prop.showdate, prop.showsec, prop.showfrm, prop.fontsize, prop.fillcolor, prop.pencolor, prop.alpha);
	fclose(fp);
}

// プロパティを読み込む
static void loadProps()
{
	int ccount = wcslen(prgPath) + 4 + 1;
	WCHAR *fname = (WCHAR *)malloc(sizeof(WCHAR) * ccount);
	if (fname == NULL) {
		return;
	}
	wcscpy_s(fname, ccount, prgPath);

	WCHAR *p = wcsrchr(fname, L'.');
	if (p == NULL) {
		wcscat_s(fname, ccount, L".ini");
	} else {
		wcscpy_s(p, ccount - (p - fname), L".ini");
	}

	FILE *fp;
	if (_wfopen_s(&fp, fname, L"r") != 0) {
		free(fname);
		return;
	}
	free(fname);

	POS pos;
	int lines, showdate, showsec, showfrm, fontsize, alpha;
	unsigned long fillcolor, pencolor;
	int n = fwscanf_s(fp, L"%d\n%d\n%d\n%d\n%d\n%d\n%lu\n%lu\n%d\n",
				&pos, &lines, &showdate, &showsec, &showfrm, &fontsize, &fillcolor, &pencolor, &alpha);
	fclose(fp);
	if (n == 9) {
		prop.pos = pos;
		prop.lines = lines;
		prop.showdate = (showdate & 1);
		prop.showsec = (showsec & 1);
		prop.showfrm = (showfrm & 1);
		prop.fontsize = fontsize;
		prop.fillcolor = fillcolor;
		prop.pencolor = pencolor;
		prop.alpha = alpha;
	}
}
