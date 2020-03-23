#pragma once


void debug(const wchar_t* format, ...);

void printLastError(const WCHAR* msg);

void xxd(const uint8_t* buffer, uint64_t address, uint32_t size);
