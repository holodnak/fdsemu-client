#if defined(_WIN32)

#include <windows.h>
#include <stdint.h>
#include <conio.h>

uint32_t getTicks() {
	return GetTickCount();
}

void utf8_to_utf16(uint16_t *dst, char *src, size_t dstSize) {
	MultiByteToWideChar(CP_ACP, 0, src, -1, (wchar_t*)dst, dstSize / sizeof(wchar_t));
}

char readKb() {
	return _getch();
}

void sleep_ms(int millisecs) {
	Sleep(millisecs);
}

#elif defined(__linux__) || defined(__APPLE__)

#include <sys/time.h>
#include <string.h>
#include <iconv.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

uint32_t getTicks() {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (unsigned long)((tv.tv_sec * 1000ul) + (tv.tv_usec / 1000ul));
}

void utf8_to_utf16(uint16_t *dst, char *src, size_t dstSize) {
	size_t srcSize = strlen(src) + 1;
	iconv_t ic;
	ic = iconv_open("UTF-16LE", "UTF-8");
	if (ic == (iconv_t)-1)
		return;
	iconv(ic, &src, &srcSize, (char**)&dst, &dstSize);
	iconv_close(ic);
}

int readKb() {
	struct termios oldt, newt;
	int ch;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}

void sleep_ms(int millisecs) {
	usleep(millisecs * 1000);
}

#endif
