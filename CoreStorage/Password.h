#pragma once


std::unique_ptr<uint8_t, free_delete> getPassword(const wchar_t* format, ...);
