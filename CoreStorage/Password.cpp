// Password.cpp : Password I/O.
//

#include "stdafx.h"


std::unique_ptr<uint8_t, free_delete> getPassword(const wchar_t* format, ...)
{
	va_list arg_list;
	va_start(arg_list, format);
	vfwprintf(stdout, format, arg_list);
	va_end(arg_list);

#ifdef _WIN32
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode = 0;
	GetConsoleMode(hStdin, &mode);
	SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
#else
	termios oldt;
	tcgetattr(STDIN_FILENO, &oldt);
	termios newt = oldt;
	newt.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif

	std::string s;
	std::getline(std::cin, s);

#ifdef _WIN32
	SetConsoleMode(hStdin, mode);
#else
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

	std::cout << "\n";

	std::unique_ptr<uint8_t, free_delete>  passwd((uint8_t*)malloc(s.length() + 1));
	memcpy(passwd.get(), s.c_str(), s.length() + 1);

	// FIXME: zero out string
	return passwd;
}
