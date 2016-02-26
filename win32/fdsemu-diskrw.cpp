// fdsemu-diskrw.cpp : Defines the entry point for the application.
//

#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "comctl32.lib")

//mingw needs this
#define _WIN32_IE 0x0300
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
//#include <vld.h>
#include "fdsemu-diskrw.h"
#include "Device.h"
#include "flashrw.h"
#include "diskutil.h"
#include "Disk.h"
#include "Fdsemu.h"

#define MAX_LOADSTRING 100

#define ID_QUIT			2
#define ID_STATUS			10
#define ID_DISKLIST		11

// Global Variables:
HINSTANCE hInst;                                // current instance
CHAR szTitle[MAX_LOADSTRING];                  // The title bar text
CHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

															  // Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ReadDiskDlg(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    WriteDiskDlg(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    WriteImagesDlg(HWND, UINT, WPARAM, LPARAM);

CDevice dev;

int get_mcu_clock()
{
	return(dev.Clock);
}

//CDisk disk;

#include <io.h>
#ifndef F_OK
#define F_OK 00
#define R_OK 04
#endif

bool file_exists(char *fn)
{
	if (_access(fn, F_OK) != -1) {
		return(true);
	}
	return(false);
}

void CheckMessages()
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

CFdsemu *fdsemu;

typedef struct SStringEntry {
	char *str;
	struct SStringEntry *next;
} TStringEntry;

class CStringList {
protected:
	TStringEntry *head;

public:
	CStringList();
	~CStringList();

	void Add(char *str);
	char *Get(int index);
	int Count();
	void Clear();
};

CStringList::CStringList()
{
	head = 0;
}

CStringList::~CStringList()
{
	Clear();
}

void CStringList::Add(char *str)
{
	TStringEntry *se, *s;

	se = new TStringEntry;
	se->str = _strdup(str);
	se->next = 0;

	if (head == 0) {
		head = se;
	}
	else {
		s = head;
		while (s->next) {
			s = s->next;
		}
		s->next = se;
	}
}

char *CStringList::Get(int index)
{
	TStringEntry *s = head;
	char *ret = 0;

	while (s && index >= 0) {
		ret = s->str;
		index--;
		s = s->next;
	}
	return(ret);
}

int CStringList::Count()
{
	TStringEntry *s = head;
	int n = 0;

	while (s) {
		s = s->next;
		n++;
	}
	return(n);
}

void CStringList::Clear()
{
	TStringEntry *ps, *s = head;

	while (s) {
		free(s->str);
		s->str = 0;
		ps = s;
		s = s->next;
		delete ps;
	}
}

#define MAX_FILES	1000

void SelectDiskImages(HWND hWnd, CStringList *sl)
{
	OPENFILENAME ofn;       // common dialog box structure
	char *buf;
	int len;
	int bufsize = MAX_FILES * (MAX_PATH + 1) + 1;

	buf = (char*)malloc(bufsize);
	memset(buf, 0, bufsize);

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = bufsize;
	ofn.lpstrFilter = "Disk images (*.fds, *.A)\0*.fds;*.A\0All files (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	sl->Clear();
	if (GetOpenFileName(&ofn) == TRUE) {
		char *ptr = buf;
		int n = strlen(ptr);
		char tmp[1024];
		char path[1024];

		//detect if one file was selected
		if (ptr[n + 1] == NULL) {
			sl->Add(ptr);
		}

		//multiple files selected
		else {

			//save file path
			strcpy(path, ptr);

			//skip the pathname part of the buffer returned
			ptr += strlen(ptr) + 1;

			while (*ptr != NULL) {
				sprintf(tmp, "%s\\%s", path, ptr);
				sl->Add(tmp);
				len = strlen(ptr);
				ptr += len + 1;
			}
		}

//		sprintf(tmp, "%d files", sl->Count());
//		MessageBox(hWnd, tmp, "erererer", MB_OK);
	}

	free(buf);
}

void WriteCallback(void *user, int x, int y)
{
	/*
	x = 0, number of sides total, called before anything is written
	x = 1, current side number writing, calling before that side has started
	x = 2, write address of current side, called periodically
	*/
	HWND hProgress = (HWND)user;
	static int curside = 0;

	CheckMessages();

	//number of sides total
	if (x == 0) {
		SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, (y * 0x10000) / 0x100));
		SendMessage(hProgress, PBM_SETSTEP, (WPARAM)1, 0);
		curside = 0;
	}

	else if (x == 1) {
		curside = y;
	}

	else if (x == 2) {
		SendMessage(hProgress, PBM_SETPOS, (WPARAM)((y + (curside * 0x10000)) / 0x100), 0);
	}

}

void WriteDiskImages(HWND hWnd, CStringList *sl)
{
	int i;
	HWND hDlg;
	HWND hList, hProgress, hProgress2;

	BOOL b = EnableWindow(hWnd, FALSE);
	hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_WRITEDIALOG), hWnd, reinterpret_cast<DLGPROC>(WriteImagesDlg), (LPARAM)sl);
	hList = GetDlgItem(hDlg, IDC_WRITELIST);
	hProgress = GetDlgItem(hDlg, IDC_PROGRESS);
	ShowWindow(hDlg, SW_SHOW);
	for (i = 0; i < sl->Count(); i++) {
		SendMessage(hList, LB_ADDSTRING, 0, (WPARAM)sl->Get(i));
		CheckMessages();
		fdsemu->WriteFlash(sl->Get(i), WriteCallback, (HWND)hProgress);
		fdsemu->dev->FlashUtil->ReadHeaders();
	}
	DestroyWindow(hDlg);
	EnableWindow(hWnd, TRUE);
	SetActiveWindow(hWnd);
}

void InsertListViewColumns(HWND hList)
{
	LVCOLUMN lvc;

	memset((void*)&lvc, 0, sizeof(LVCOLUMN));
	lvc.mask = LVCF_FMT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.pszText = (LPSTR)"";
	lvc.cchTextMax = 90;
	ListView_InsertColumn(hList, 1, &lvc);

}

BOOL InsertListViewItem(HWND hList, char *name, int id)
{
	LVITEM lvi;

	memset((void*)&lvi, 0, sizeof(LVITEM));
	lvi.mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
	lvi.pszText = name;
	lvi.lParam = id;
	if (ListView_InsertItem(hList, &lvi) == -1) {
		return FALSE;
	}

	return TRUE;
}

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	TFlashHeader *headers = fdsemu->dev->FlashUtil->GetHeaders();

	return(_stricmp((char*)headers[lParam1].filename, (char*)headers[lParam2].filename));
}

int GenerateList(HWND hList,HWND hStatus)
{
	TFlashHeader *headers;
	uint32_t i;
	int side = 0, empty = 0;
	char str[128];

	ListView_DeleteAllItems(hList);
	EnableWindow(hList, FALSE);
	SetWindowText(hStatus, "Reading disk headers...");
	CheckMessages();

	fdsemu->dev->FlashUtil->ReadHeaders();
	headers = fdsemu->dev->FlashUtil->GetHeaders();
	if (headers == 0) {
		return(-1);
	}

	for (i = 0; i < fdsemu->dev->Slots; i++) {
		TFlashHeader *header = &headers[i];
		uint8_t *buf = headers[i].filename;

		//empty slot
		if (buf[0] == 0xFF) {
			empty++;
			continue;
		}

		//this slot has valid ownerid/nextid
		if (header->flags & 0x20) {

			//first disk image of a set
			if (header->ownerid == i) {
				InsertListViewItem(hList, (char*)buf, i);
			}
		}

		else {
			//filename is here
			if (buf[0] != 0) {
				InsertListViewItem(hList, (char*)buf, i);
			}
		}
	}

	ListView_SortItems(hList, CompareFunc, 0);
	EnableWindow(hList, TRUE);

	//update teh status bar
	if (empty >= 0) {
		sprintf(str, "%d slots used, %d remaining.", fdsemu->dev->Slots - empty, empty);
	}
	else {
		sprintf(str, "Error reading flash headers.");
	}
	SetWindowText(hStatus, str);


	return(empty);
}

void DragFunc(HWND hWnd, HDROP hDrop)
{
	int i, numfiles;
	char filename[1024];
	CStringList *sl = new CStringList();

	numfiles = DragQueryFile(hDrop, 0xFFFFFFFF, (LPSTR)NULL, 0);
	for (i = 0; i < numfiles; i++) {
		DragQueryFile(hDrop, i, filename, sizeof(filename));
		sl->Add(filename);
	}

	WriteDiskImages(hWnd, sl);
	delete sl;

	//display new list of disks stored in flash
	i = GenerateList(GetDlgItem(hWnd, ID_DISKLIST), GetDlgItem(hWnd, ID_STATUS));
}

int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR    lpCmdLine,
	int       nCmdShow)
{
	char str[1024];

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	fdsemu = new CFdsemu(&dev);
	if (fdsemu->Init() == false) {
		fdsemu->GetError(str, 1024);
		MessageBox(0, str, "Error", MB_OK);
		return(0);
	}

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_FDSEMUDISKRW, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow)) {
		fdsemu->dev->Close();
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FDSEMUDISKRW));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	fdsemu->dev->Close();

	delete fdsemu;

	return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 0);
	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_FDSEMUDISKRW);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	INITCOMMONCONTROLSEX iccx;

	hInst = hInstance; // Store instance handle in our global variable

	iccx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccx.dwICC = ICC_BAR_CLASSES;
	if (!InitCommonControlsEx(&iccx))
		return FALSE;

	HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 500, 600, NULL, NULL, hInstance, NULL);

	if (!hWnd) {
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HFONT hFont = NULL;
	HWND hReadButton = NULL;
	HWND hStatus = NULL;
	HWND hList = NULL;
	int i;
	static char str[512];
	LVITEM lvi;
	CStringList *sl;

	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case ID_DISK_READDISK:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_READDISK), hWnd, reinterpret_cast<DLGPROC>(ReadDiskDlg));
			break;

		case ID_DISK_WRITEDISK:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_WRITEDISK), hWnd, reinterpret_cast<DLGPROC>(WriteDiskDlg));
			break;

		case ID_FLASH_WRITEIMAGES:
			sl = new CStringList();
			SelectDiskImages(hWnd,sl);
			WriteDiskImages(hWnd, sl);
			delete sl;
			GenerateList(GetDlgItem(hWnd, ID_DISKLIST),GetDlgItem(hWnd, ID_STATUS));
			break;

		case ID_POPUP_DELETE:
			hStatus = GetDlgItem(hWnd, ID_STATUS);
			hList = GetDlgItem(hWnd, ID_DISKLIST);

			//find selected items
			i = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
			while (i != -1) {
				lvi.iItem = i;
				lvi.iSubItem = 0;
				lvi.mask = LVIF_PARAM;
				if (ListView_GetItem(hList, &lvi) == TRUE) {

					//update status bar
					sprintf(str, "Deleting disk image in slot %d from flash...", lvi.lParam);
					SetWindowText(hStatus, str);

					fdsemu->Erase(lvi.lParam);
				}

				//check if more are selected
				i = ListView_GetNextItem(hList, i, LVNI_SELECTED);
			}

			//re-read the headers
			fdsemu->dev->FlashUtil->ReadHeaders();

			//display new list of disks stored in flash
			i = GenerateList(hList, hStatus);

			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_CREATE:
		hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

		SendMessage(hWnd, WM_SETFONT, (WPARAM)hFont, (LPARAM)MAKELONG(TRUE, 0));

		hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL,
			WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
			hWnd, (HMENU)ID_STATUS, GetModuleHandle(NULL), NULL);

		hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | LVS_REPORT | LVS_EDITLABELS | LVS_NOCOLUMNHEADER,
			10, 10, 300, 300,
			hWnd, (HMENU)ID_DISKLIST, GetModuleHandle(NULL), NULL);

		InsertListViewColumns(hList);
		i = GenerateList(hList,hStatus);
		if (i >= 0) {
			sprintf(str, "%d slots used, %d remaining.", fdsemu->dev->Slots - i, i);
		}
		else {
			sprintf(str, "Error reading flash headers.");
		}
		SetWindowText(hStatus, str);
		DragAcceptFiles(hWnd, TRUE);
		break;

	case WM_DESTROY:
		DragAcceptFiles(hWnd, FALSE);
		PostQuitMessage(0);
		break;

	case WM_DROPFILES:
		DragFunc(hWnd, (HDROP)wParam); /* application-defined function */
		break;

	case WM_NOTIFY:
		hList = GetDlgItem(hWnd, ID_DISKLIST);

		// When right button clicked on mouse
		if ((((LPNMHDR)lParam)->hwndFrom) == hList) {
			if (ListView_GetSelectedCount(hList) > 0) {
				switch (((LPNMHDR)lParam)->code) {
				case NM_RCLICK:
					POINT cursor;
					GetCursorPos(&cursor);
					TrackPopupMenu((HMENU)GetSubMenu(LoadMenu(hInst, MAKEINTRESOURCE(IDR_DISKLISTMENU)), 0), TPM_LEFTALIGN | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hWnd, NULL);
					return(0);
				}
			}
		}
		return DefWindowProc(hWnd, message, wParam, lParam);

	case WM_SIZE:
		RECT rcStatus;
		int iStatusHeight;
		int iEditHeight;
		RECT rcClient;

		// Size status bar and get height

		hStatus = GetDlgItem(hWnd, ID_STATUS);
		SendMessage(hStatus, WM_SIZE, 0, 0);

		GetWindowRect(hStatus, &rcStatus);
		iStatusHeight = rcStatus.bottom - rcStatus.top;

		// Calculate remaining height and size edit

		GetClientRect(hWnd, &rcClient);

		iEditHeight = rcClient.bottom - iStatusHeight;

		hList = GetDlgItem(hWnd, ID_DISKLIST);
		SetWindowPos(hList, NULL, 0, 0, rcClient.right, iEditHeight, SWP_NOZORDER);

		ListView_SetColumnWidth(hList, 0, rcClient.right - 22);

		break;
	case WM_GETMINMAXINFO:
		MINMAXINFO* mmi;
		mmi = (MINMAXINFO*)lParam;
		mmi->ptMinTrackSize.x = 400;
		mmi->ptMinTrackSize.y = 400;
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK WriteImagesDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int slot;

	switch (message)
	{
	case WM_INITDIALOG:
		slot = lParam;
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
