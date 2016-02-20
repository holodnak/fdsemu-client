// fdsemu-diskrw.cpp : Defines the entry point for the application.
//

#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "comctl32.lib")

//mingw needs this
#define _WIN32_IE 0x0300
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "fdsemu-diskrw.h"
#include "Device.h"
#include "flashrw.h"

#define MAX_LOADSTRING 100

#define ID_QUIT			2
#define ID_STATUS		10
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

CDevice dev;

//allocate buffer and read whole file
bool loadfile(char *filename, uint8_t **buf, int *filesize)
{
	FILE *fp;
	int size;
	bool result = false;

	//check if the pointers are ok
	if (buf == 0 || filesize == 0) {
		return(false);
	}

	//open file
	if ((fp = fopen(filename, "rb")) == 0) {
		return(false);
	}

	//get file size
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	//allocate buffer
	*buf = (uint8_t*)malloc(size);

	//read in file
	*filesize = fread(*buf, 1, size, fp);

	//close file and return
	fclose(fp);
	return(true);
}

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

typedef struct disk_s {
	uint8_t *raw;
	int rawsize;
	int datasize;
} disk_t;

enum {
	FORMAT_RAW = 0,
	FORMAT_BIN,
	FORMAT_FDS,
	FORMAT_GD,
	FORMAT_SMC
};

disk_t disks[16];
int numdisks;

class CFdsemu {
private:
	char error[1024];
	bool success;

protected:
	void SetError(const char *str);
	void ClearError();

public:
	CDevice *dev;

public:
	CFdsemu(CDevice *d);
	~CFdsemu();

	bool Init();
	bool CheckDevice();
	bool GetError(char *str, int len);
	bool GetError(wchar_t *wstr, int len);
	bool ReadDisk(uint8_t **raw, int *rawsize);
	int ParseDiskData(uint8_t *raw, int rawsize, char **output);
	int WriteFlash(char *filename, int(*callback)(void*, int, int), void *user);
};

void CFdsemu::SetError(const char *str)
{
	strcat(error, str);
	success = false;
}

void CFdsemu::ClearError()
{
	memset(error, 0, 1024);
	success = true;
}

CFdsemu::CFdsemu(CDevice *d)
{
	dev = d;
	ClearError();
}

CFdsemu::~CFdsemu()
{
	dev->Close();
}

bool CFdsemu::Init()
{
	bool ret = dev->Open();

	if (ret == true) {
		dev->FlashUtil->ReadHeaders();
	}
	SetError("Error opening device.  Is the FDSemu plugged in?\n");
	return(ret);
}

bool CFdsemu::CheckDevice()
{
	bool ret;

	ret = dev->Reopen();
	if (ret == false) {
		SetError("FDSemu not detected, please re-insert the device.\n");
	}
	return(ret);
}

bool CFdsemu::GetError(char *str, int len)
{
	strncpy(str, error, len);
	return(success);
}

bool CFdsemu::GetError(wchar_t *wstr, int len)
{
	mbstowcs(wstr, error, len);
	return(success);
}

bool CFdsemu::ReadDisk(uint8_t **raw, int *rawsize)
{
	enum { 
		READBUFSIZE = 0x90000,
		LEADIN = 26000
	};

//	FILE *f;
	uint8_t *readBuf = NULL;
	int result;
	int bytesIn = 0;

	*raw = 0;
	*rawsize = 0;
	if (CheckDevice() == false) {
		return(false);
	}
	ClearError();

	//if(!(dev_readIO()&MEDIA_SET)) {
	//    printf("Warning - Disk not inserted?\n");
	//}
	if (!dev->DiskReadStart()) {
		SetError("diskreadstart failed\n");
		return false;
	}

	readBuf = (uint8_t*)malloc(READBUFSIZE);
	do {
		result = dev->DiskRead(readBuf + bytesIn);
		bytesIn += result;
//		if (!(bytesIn % ((DISK_READMAX)* 32)))
//			printf(".");
	} while (result == DISK_READMAX && bytesIn<READBUFSIZE - DISK_READMAX);

	if (result<0) {
		SetError("read error\n");
		free(readBuf);
		return false;
	}

	//eat up the lead-in
	*raw = (uint8_t*)malloc(bytesIn - (LEADIN / 8));
	*rawsize = bytesIn - (LEADIN / 8);
	memcpy(*raw, readBuf + (LEADIN / 8), *rawsize);

	free(readBuf);

	return true;
}

#include "diskutil.h"

int CFdsemu::ParseDiskData(uint8_t *raw, int rawsize, char **output)
{
	uint8_t *binBuf;
	int binSize, datasize;
	bool ret = false;

	raw_to_bin(raw, rawsize, &binBuf, &binSize, &datasize);
	free(binBuf);
	messages_printf("Total size of disk data: %d bytes\r\n", datasize);
	*output = messages_get();
	return(datasize);
}

//write disk image to flash
int CFdsemu::WriteFlash(char *filename, int(*callback)(void*, int, int), void *user)
{
	enum { FILENAMELENGTH = 240, };   //number of characters including null

	uint8_t *inbuf = 0;
	uint8_t *outbuf = 0;
	int filesize;
	char *shortName;
	TFlashHeader *headers = dev->FlashUtil->GetHeaders();
	int pos = 0, side = 0;
	int slot;

	if (headers == 0) {
		printf("error reading flash headers");
		return(false);
	}

	if (!loadfile(filename, &inbuf, &filesize)) {
		printf("Can't read %s\n", filename);
		return false;
	}

	if (inbuf[0] == 'F') {
		pos = 16;      //skip fwNES header
	}

	filesize -= (filesize - pos) % FDSSIZE;  //truncate down to whole disks
						
	shortName = get_shortname(filename);

	slot = find_slot(filesize / FDSSIZE);

	if (slot == -1) {
		printf("Cannot find %d adjacent slots for storing disk image.\nPlease make room on the flash to store this disk image.\n", filesize / FDSSIZE);
		delete[] inbuf;
		return(false);
	}

	printf("Writing disk image to flash slot %d...\n", slot);

	outbuf = new uint8_t[SLOTSIZE];

	while (pos<filesize && inbuf[pos] == 0x01) {
		printf("Side %d", side + 1);
		if (fds_to_bin(outbuf + FLASHHEADERSIZE, inbuf + pos, SLOTSIZE - FLASHHEADERSIZE)) {
			memset(outbuf, 0, FLASHHEADERSIZE);
			uint32_t chksum = chksum_calc(outbuf + FLASHHEADERSIZE, SLOTSIZE - FLASHHEADERSIZE);

			//write leadin
			outbuf[244] = DEFAULT_LEAD_IN & 0xff;
			outbuf[245] = DEFAULT_LEAD_IN / 256;
			outbuf[250] = 0;

			if (side == 0) {
				strncpy((char*)outbuf, shortName, 240);
			}
			if (dev->Flash->Write(outbuf, (slot + side)*SLOTSIZE, SLOTSIZE) == false) {
				printf("error.\n");
				break;
			}
			printf("done.\n");
		}
		pos += FDSSIZE;
		side++;
	}
	delete[] inbuf;
	delete[] outbuf;
	printf("\n");
	return true;

	return(0);
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

int GenerateList(HWND hList)
{
	TFlashHeader *headers = fdsemu->dev->FlashUtil->GetHeaders();
	uint32_t i;
	int side = 0, empty = 0;

	ListView_DeleteAllItems(hList);
	if (headers == 0) {
		return(-1);
	}

	printf("Listing disks stored in flash:\n");

	for (i = 0; i < fdsemu->dev->Slots; i++) {
		uint8_t *buf = headers[i].filename;

		if (buf[0] == 0xFF) {          //empty
			empty++;
		}

		//filename is here
		else if (buf[0] != 0) {
			InsertListViewItem(hList, (char*)buf, i);
		}
	}
	ListView_SortItems(hList, CompareFunc, 0);
	return(empty);
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

	// TODO: Place code here.
/*	if (dev.Open() == false) {
		MessageBox(0, L"Error detecting FDSemu", L"Error", MB_OK);
		return(2);
	}*/

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_FDSEMUDISKRW, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
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
	return (int) msg.wParam;
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

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+0);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_FDSEMUDISKRW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));

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
				//reset all disk reading information data
				memset(disks, 0, sizeof(disk_t) * 16);
				numdisks = 0;

				DialogBox(hInst, MAKEINTRESOURCE(IDD_READDISK), hWnd, reinterpret_cast<DLGPROC>(ReadDiskDlg));

				for (i = 0; i < 16; i++) {
					if (disks[i].raw) {
						free(disks[i].raw);
					}
					disks[i].raw = 0;
					disks[i].rawsize = 0;
				}
				numdisks = 0;
				break;

			case ID_DISK_WRITEDISK:
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

						fdsemu->dev->Flash->EraseSlot(lvi.lParam);
					}

					//check if more are selected
					i = ListView_GetNextItem(hList, i, LVNI_SELECTED);
				}

				//re-read the headers
				fdsemu->dev->FlashUtil->ReadHeaders();

				//display new list of disks stored in flash
				i = GenerateList(hList);

				//update teh status bar
				if (i >= 0) {
					sprintf(str, "%d slots used, %d remaining.", fdsemu->dev->Slots - i, i);
				}
				else {
					sprintf(str, "Error reading flash headers.");
				}
				SetWindowText(hStatus, str);
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
		i = GenerateList(hList);
		if (i >= 0) {
			sprintf(str, "%d slots used, %d remaining.", fdsemu->dev->Slots - i, i);
		}
		else {
			sprintf(str, "Error reading flash headers.");
		}
		SetWindowText(hStatus, str);

		break;
	case WM_DESTROY:
        PostQuitMessage(0);
        break;
	case WM_NOTIFY:
		hList = GetDlgItem(hWnd, ID_DISKLIST);

		// When right button clicked on mouse
		if ((((LPNMHDR)lParam)->hwndFrom) == hList) {
			if(ListView_GetSelectedCount(hList) > 0) {
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

void OutputInformation(HWND hDlg, int side)
{
	int i;
	HWND hRadioFDS, hEdit, hStatic;
	char *output = 0;

	disks[side].datasize = fdsemu->ParseDiskData(disks[side].raw, disks[side].rawsize, &output);

	hRadioFDS = GetDlgItem(hDlg, IDC_RADIO_FDS);
	hEdit = GetDlgItem(hDlg, IDC_READEDIT);

	for (i = 0; i < numdisks; i++) {
		if (disks[i].datasize > 65500) {
			EnableWindow(hRadioFDS, FALSE);
			CheckRadioButton(hDlg, IDC_RADIO_FDS, IDC_RADIO_GD, IDC_RADIO_GD);
		}
	}

	if (output && strlen(output)) {
		char str[512];
		int len = strlen(output);

		SetWindowText(hEdit, output);

		memset(str, 0, 512);
		wsprintf(str, "Side %d\r\n\r\nData size: %d bytes\r\nFormat: %s", 
			side + 1, disks[side].datasize, disks[side].datasize > 65500 ? "Game Doctor" : "fwNES");
		hStatic = GetDlgItem(hDlg, IDC_READINFO);
		SetWindowText(hStatic, str);
	}
}

void SaveDiskImage_FDS(char *fn)
{
	FILE *fp;
	uint8_t *raw03, *out;
	int i, rawsize, outsize;

	//try to open file
	if ((fp = fopen(fn, "wb")) == 0) {
		MessageBox(0, "Error opening output file.", "Error", MB_OK);
		return;
	}

	out = (uint8_t*)malloc(1024 * 1024);

	//convert each disk side
	for (i = 0; i < numdisks; i++) {

		memset(out, 0, 1024 * 1024);

		//save raw data size
		rawsize = disks[i].rawsize;

		//convert raw to raw03
		raw03 = (uint8_t*)malloc(rawsize);
		memcpy(raw03, disks[i].raw, rawsize);
		raw_to_raw03(raw03, rawsize);

		outsize = 65500;
		raw03_to_fds(raw03, out, rawsize);
		free(raw03);

		fwrite(out, 1, outsize, fp);
	}

	free(out);

	//close file
	fclose(fp);
}

void SaveDiskImage_GD(char *filename)
{
	char fn[1024];
	FILE *fp;
	uint8_t *raw03, *out;
	int i, rawsize, outsize;

	//convert to this char* array
	memset(fn, 0, 1024);
	strncpy(fn, filename, 1024);

	out = (uint8_t*)malloc(1024 * 1024);

	//convert each disk side
	for (i = 0; i < numdisks; i++) {

		memset(out, 0, 1024 * 1024);

		//save raw data size
		rawsize = disks[i].rawsize;

		//convert raw to raw03
		raw03 = (uint8_t*)malloc(rawsize);
		memcpy(raw03, disks[i].raw, rawsize);
		raw_to_raw03(raw03, rawsize);

		outsize = raw03_to_gd(raw03, out, rawsize);
		free(raw03);

		//try to open file
		if ((fp = fopen(fn, "wb")) == 0) {
			MessageBox(0, "Error opening output file.", "Error", MB_OK);
			return;
		}

		fwrite(out, 1, outsize, fp);

		//close file
		fclose(fp);

		//increment file extension
		fn[strlen(fn) - 1]++;
	}

}

INT_PTR CALLBACK ReadDiskDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hEdit, hList, hButton, hStatic;
	uint8_t *raw;
	int rawsize, pos;
	char str[512];
	char *output = 0;
	int lbItem, i;

	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		hButton = GetDlgItem(hDlg, IDC_REREADBUTTON);
		EnableWindow(hButton, FALSE);
		hStatic = GetDlgItem(hDlg, IDC_READINFO);
		SetWindowText(hStatic, "");
		CheckRadioButton(hDlg, IDC_RADIO_FDS, IDC_RADIO_GD, IDC_RADIO_FDS);
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SIDELIST:
			switch (HIWORD(wParam)) {
			case LBN_SELCHANGE:
				hButton = GetDlgItem(hDlg, IDC_REREADBUTTON);
				EnableWindow(hButton, TRUE);
				hList = GetDlgItem(hDlg, IDC_SIDELIST);

				// Get selected index.
				lbItem = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);

				// Get item data.
				i = (int)SendMessage(hList, LB_GETITEMDATA, lbItem, 0);

				if (i >= numdisks)
					return TRUE;

				OutputInformation(hDlg, i);

				return TRUE;
			}
			return (INT_PTR)TRUE;

		case IDC_READBUTTON:
			hEdit = GetDlgItem(hDlg, IDC_READEDIT);

			sprintf(str, "Reading disk side %d...", numdisks + 1);
			SetWindowText(hEdit, str);

			CheckMessages();

			if (fdsemu->ReadDisk(&raw, &rawsize) == false) {
				fdsemu->GetError(str, 512);
				MessageBox(0, str, "Error", MB_OK);
			}
			else {
				disks[numdisks].raw = raw;
				disks[numdisks].rawsize = rawsize;

				hList = GetDlgItem(hDlg, IDC_SIDELIST);
				sprintf(str, "Side %d", numdisks + 1);
				pos = (int)SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)str);
				SendMessage(hList, LB_SETITEMDATA, pos, (LPARAM)numdisks);
				SendMessage(hList, LB_SETSEL, (WPARAM)(BOOL)TRUE, (LPARAM)pos);

				numdisks++;

				//this will fill in the datasize member
				OutputInformation(hDlg, numdisks - 1);

			}

			return (INT_PTR)TRUE;
		case IDC_REREADBUTTON:
			hList = GetDlgItem(hDlg, IDC_SIDELIST);
			hEdit = GetDlgItem(hDlg, IDC_READEDIT);

			// Get selected index.
			lbItem = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);

			// Get item data.
			i = (int)SendMessage(hList, LB_GETITEMDATA, lbItem, 0);

			sprintf(str, "Re-reading disk side %d...", i + 1);
			SetWindowText(hEdit, str);

			CheckMessages();

			if (fdsemu->ReadDisk(&raw, &rawsize) == false) {
				fdsemu->GetError(str, 512);
				MessageBox(0, str, "Error", MB_OK);
			}
			else {
				sprintf(str, "Read %d bytes", rawsize);
				SetWindowText(hEdit, str);

				free(disks[i].raw);
				disks[i].raw = raw;
				disks[i].rawsize = rawsize;

				OutputInformation(hDlg, i);
			}

			return (INT_PTR)TRUE;
		case IDC_SAVEBUTTON:
			OPENFILENAME ofn;
			wchar_t filename[1024];

			ZeroMemory(&ofn, sizeof(ofn));
			ZeroMemory(&filename, sizeof(wchar_t) * 1024);

			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hDlg;
			if (IsDlgButtonChecked(hDlg,IDC_RADIO_FDS) == BST_CHECKED) {
				ofn.lpstrFilter = (LPCSTR)"fwNES Image (*.fds)\0*.fds\0";
				i = 0;
			}
			else {
				ofn.lpstrFilter = (LPCSTR)"Game Doctor Image (*.A)\0*.A\0";
				i = 1;
			}
			ofn.lpstrFile = (LPSTR)filename;
			ofn.nMaxFile = 1024;
			ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;

			GetSaveFileName(&ofn);

			if (i == 0) {
				SaveDiskImage_FDS(ofn.lpstrFile);
			}
			else {
				SaveDiskImage_GD(ofn.lpstrFile);
			}

			return (INT_PTR)TRUE;
		case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
