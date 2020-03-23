// Main.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"




int wmain(int argc, wchar_t* argv[])
{
    uint8_t header[512] = { 0 };
    std::unique_ptr<BlockDevice> device = open(argv[argc - 1]);

    device->read(header, 0, 512);
    xxd(header, 0, 512);

    if (('N' == header[3]) && ('T' == header[4]) && ('F' == header[5]) && ('S' == header[6]))
    {
        debug(L"Filesystem: NTFS");
        parseNTFS(device, header);
    }
    else if (('C' == header[88]) && ('S' == header[89]))
    {
        debug(L"Volume: Core Storage");
        parseCoreStorage(device, header);
    }

    return 0;
}
