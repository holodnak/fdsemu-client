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

typedef struct disk_s {
	uint8_t *raw, *bin;
	int rawsize, binsize;
	int datasize;
} disk_t;

disk_t disks[16];
int numdisks;

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

		//reset all disk reading information data
		memset(disks, 0, sizeof(disk_t) * 16);
		numdisks = 0;

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
			if (IsDlgButtonChecked(hDlg, IDC_RADIO_FDS) == BST_CHECKED) {
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

			for (i = 0; i < 16; i++) {
				if (disks[i].raw) {
					free(disks[i].raw);
				}
				memset(&disks[i], 0, sizeof(disk_t));
			}
			numdisks = 0;

			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
