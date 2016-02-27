//mingw needs this
#define _WIN32_IE 0x0300
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "fdsemu-diskrw.h"
#include "Device.h"
#include "flashrw.h"
#include "diskutil.h"
#include "Disk.h"
#include "Fdsemu.h"

static CDisk disk;

// Message handler for about box.
INT_PTR CALLBACK WriteDiskDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	const int BINSIZE = 0x18000;
	HWND hStatic, hCombo;
	char str[128];
	int i;
	uint8_t *bin;
	int binsize;

	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		disk.FreeSides();
		hStatic = GetDlgItem(hDlg, IDC_WRITEINFO);
		SetWindowText(hStatic, "Please ensure the cable is connected to FDSemu and the\r\nFDS disk drive.\r\n\r\nPress the \"Load disk image\" button to load an image to write\r\nto disk.");

		EnableWindow(GetDlgItem(hDlg, IDC_STARTBUTTON), FALSE);

		EnableWindow(GetDlgItem(hDlg, IDC_SIDECOMBO), FALSE);
		CheckRadioButton(hDlg, IDC_ALLSIDESRADIO, IDC_ONESIDERADIO, IDC_ALLSIDESRADIO);

		EnableWindow(GetDlgItem(hDlg, IDC_ALLSIDESRADIO), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_ONESIDERADIO), FALSE);

		return (INT_PTR)TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDC_ALLSIDESRADIO:
		case IDC_ONESIDERADIO:
			if (IsDlgButtonChecked(hDlg, IDC_ONESIDERADIO) == BST_CHECKED) {
				EnableWindow(GetDlgItem(hDlg, IDC_SIDECOMBO), TRUE);
			}
			else {
				EnableWindow(GetDlgItem(hDlg, IDC_SIDECOMBO), FALSE);
			}
			break;
			//			return (INT_PTR)TRUE;

		case IDC_STARTBUTTON:

			//write entire disk, side by side
			if (IsDlgButtonChecked(hDlg, IDC_ALLSIDESRADIO) == BST_CHECKED) {

				sprintf(str, "Disk is currently writing.  Do not eject the disk until disk activity stops.");
				hStatic = GetDlgItem(hDlg, IDC_WRITEINFO);
				SetWindowText(hStatic, str);

				//for writing all disk sides
				for (i = 0; i < disk.GetSides(); i++) {
					if (i > 0) {
						if (MessageBox(hDlg, "Disk is currently writing.\r\n\r\nPress OK when the drive has completed writing, or press Cancel to stop writing.", "Message", MB_OKCANCEL) == IDCANCEL) {
							break;
						}
					}

					if (disk.GetBin(i, &bin, &binsize) == false) {
						MessageBox(hDlg, "Error converting disk side to bin.  Is this a valid disk image?", "Message", MB_OK);
						break;
					}

					fdsemu->WriteDisk(bin, binsize);
				}
			}

			//write one side only
			else {
				hCombo = GetDlgItem(hDlg, IDC_SIDECOMBO);
				i = SendMessage(hCombo, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);

				if (i == CB_ERR) {
					break;
				}

				if (disk.GetBin(i, &bin, &binsize) == false) {
					MessageBox(hDlg, "Error converting disk side to bin.  Is this a valid disk image?", "Message", MB_OK);
					break;
				}

				fdsemu->WriteDisk(bin, binsize);

				sprintf(str, "Disk side %d is currently writing.  Do not eject the disk until disk activity stops.", i + 1);
				hStatic = GetDlgItem(hDlg, IDC_WRITEINFO);
				SetWindowText(hStatic, str);
			}
			return (INT_PTR)TRUE;

		case IDC_LOADBUTTON:
			OPENFILENAME ofn;       // common dialog box structure
			char szFile[512];       // buffer for file name
			char info[1024];

			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hDlg;
			ofn.lpstrFile = szFile;
			ofn.lpstrFile[0] = '\0';
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = "Disk images (*.fds, *.A)\0*.fds;*.A\0All files (*.*)\0*.*\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			ofn.lpstrInitialDir = NULL;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

			if (GetOpenFileName(&ofn) == TRUE) {
				hCombo = GetDlgItem(hDlg, IDC_SIDECOMBO);
				disk.FreeSides();
				SendMessage(hCombo, CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
				if (disk.Load(szFile) == false) {
					sprintf(info, "Error loading %s\r\nUnknown format", szFile);
				}
				else {
					sprintf(info, "Loaded %s\r\n\r\n%d sides", szFile, disk.GetSides());

					//enable the write button
					EnableWindow(GetDlgItem(hDlg, IDC_STARTBUTTON), TRUE);

					EnableWindow(GetDlgItem(hDlg, IDC_ALLSIDESRADIO), TRUE);
					EnableWindow(GetDlgItem(hDlg, IDC_ONESIDERADIO), TRUE);

					for (i = 0; i < disk.GetSides(); i++) {
						sprintf(str, "Side %d", i + 1);
						SendMessage(hCombo, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)str);
					}
					SendMessage(hCombo, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
				}
				SetWindowText(GetDlgItem(hDlg, IDC_WRITEINFO), info);
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
