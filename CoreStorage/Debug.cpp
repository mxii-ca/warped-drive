// Debug.cpp : Debugging Utilities.
//

#include "stdafx.h"


void debug(const wchar_t* format, ...)
{
    va_list arg_list;
    va_start(arg_list, format);
    vfwprintf(stderr, format, arg_list);
    va_end(arg_list);
    fwprintf(stderr, L"\r\n");
    fwprintf(stderr, L"\r\n");
}


void printLastError(const WCHAR* msg)
{
    DWORD	error = 0;
    WCHAR*	message = NULL;
    DWORD cbMessage = 0;

    error = GetLastError();

    cbMessage = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error,
        LANG_NEUTRAL, (WCHAR*)&message, 0, NULL);
    if (cbMessage)
    {
        message[wcslen(message) - 2] = L'\0';	// remove \r\n
        wprintf(L"%ls %ls (%lu)\r\n", msg, message, error);
        (void)LocalFree(message);
    }
    else
    {
        wprintf(L"%ls GetLastError() = %lu\r\n", msg, error);
    }
}


void xxd(const uint8_t* buffer, uint64_t address, uint32_t size)
{
    uint32_t    i = 0;
    uint32_t    j = 0;

    // 00000000: xx xx xx xx xx xx xx xx - xx xx xx xx xx xx xx xx  ................
    for (; i < size; i += 16)
    {
        fwprintf(stderr, L"%08llx:", address + i);
        for (j = 0; (j < 8) && ((i + j) < size); j++)
        {
            fwprintf(stderr, L" %02hx", buffer[i + j]);
        }
        fwprintf(stderr, L" -");
        for (j = 8; (j < 16) && ((i + j) < size); j++)
        {
            fwprintf(stderr, L" %02hx", buffer[i + j]);
        }
        for (j = i + 16; j > size; j--)
        {
            fwprintf(stderr, L"   ");
        }
        fwprintf(stderr, L"  ");
        for (j = 0; (j < 16) && ((i + j) < size); j++)
        {
            if ((buffer[i + j] < 32) || (buffer[i + j] > 126))
            {
                fwprintf(stderr, L".");
            }
            else
            {
                fwprintf(stderr, L"%hc", (char)buffer[i + j]);
            }
        }
        fwprintf(stderr, L"\r\n");
    }
    fwprintf(stderr, L"\r\n");
}
