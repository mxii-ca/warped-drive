// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <math.h>
#include <stdio.h>
#include <tchar.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// io
#include "Aligned.h"
#include "Debug.h"
#include "Password.h"
// encoding
#include "base64.h"
// encryption
#include "AES.h"
#include "Hash.h"
#include "HMAC.h"
#include "KEK.h"
#include "PBKDF.h"
#include "SHA.h"
// xml
#include "plist.h"

// filesystems
#include "CoreStorage.h"
#include "NTFS.h"
