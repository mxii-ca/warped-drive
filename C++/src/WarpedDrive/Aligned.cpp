// Aligned.cpp : Aligned I/O.
//

#include "stdafx.h"


uint32_t BlockDevice::getSectorSize()
{
    if (0 == _cbSector)
    {
        _cbSector = _getSectorSize();

        debug(L"Sector Size: %lu", _cbSector);
    }
    return _cbSector;
}

uint32_t BlockDevice::read(void* buffer, uint64_t offset, uint32_t size)
{
    uint32_t sector_size = 0;
    uint32_t diff_offset = 0;
    uint32_t real_size = 0;
    uint32_t read = 0;

    sector_size = getSectorSize();

    diff_offset = offset % sector_size;
    real_size = size + diff_offset;
    if ((offset + size) % sector_size)
    {
        real_size += sector_size - ((offset + size) % sector_size);
    }
    {
        std::unique_ptr<uint8_t, free_delete>  sectors((uint8_t*)malloc(real_size));

        debug(L"Requested - Offset: %llu  Size: %lu\r\n"
            L"Alligned  - Offset: %llu  Size: %lu",
            offset, size, offset - diff_offset, real_size);

        read = _read(sectors.get(), offset - diff_offset, real_size);

        if (read <= diff_offset)
        {
            return 0;
        }
        read -= diff_offset;
        read = min(read, size);

        memcpy(buffer, sectors.get() + diff_offset, read);
    }
    return read;
}


#ifdef _WIN32
class WindowsBlockDevice : public BlockDevice
{
    HANDLE	_hDevice = INVALID_HANDLE_VALUE;

public:

    WindowsBlockDevice(WCHAR* path)
    {
        debug(L"Opening: %ls", path);

        _hDevice = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
        if (INVALID_HANDLE_VALUE == _hDevice) {
            printLastError(L"CreateFileW Failed!");
            exit(1);
        }
    }

    ~WindowsBlockDevice()
    {
        (void)CloseHandle(_hDevice);
    }

protected:

    virtual uint32_t _getSectorSize()
    {
        STORAGE_PROPERTY_QUERY	query = { StorageAccessAlignmentProperty, PropertyStandardQuery, 0 };
        STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR	alignment = { 0 };
        DWORD	cbAlignment = 0;

        if (!DeviceIoControl(_hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &alignment, sizeof(alignment), &cbAlignment, NULL))
        {
            if (ERROR_INVALID_FUNCTION != GetLastError())
            {
                printLastError(L"DeviceIoControl Failed!");
                exit(2);
            }

            // IOCTL_STORAGE_QUERY_PROPERTY doesn't work for external drives... fallback to getting the logical sector size
            else
            {
                DISK_GEOMETRY geometry = { 0 };
                DWORD   cbGeometry = 0;

                debug(L"IOCTL_STORAGE_QUERY_PROPERTY Failed: Attempting IOCTL_DISK_GET_DRIVE_GEOMETRY...");

                if (!DeviceIoControl(_hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geometry, sizeof(geometry), &cbGeometry, NULL))
                {
                    printLastError(L"DeviceIoControl Failed!");
                    exit(2);
                }

                debug(L"DISK_GEOMETRY {\r\n"
                    L"    Cylinders         = %llu\r\n"
                    L"    MediaType         = %02hx\r\n"
                    L"    TracksPerCylinder = %u\r\n"
                    L"    SectorsPerTrack   = %u\r\n"
                    L"    BytesPerSector    = %u\r\n"
                    L"}", geometry.Cylinders.QuadPart, geometry.MediaType, geometry.TracksPerCylinder, geometry.SectorsPerTrack, geometry.BytesPerSector);

                return geometry.BytesPerSector;
            }
        }

        debug(L"STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR {\r\n"
            L"    Version                       = %lu\r\n"
            L"    Size                          = %lu\r\n"
            L"    BytesPerCacheLine             = %lu\r\n"
            L"    BytesOffsetForCacheAlignment  = %lu\r\n"
            L"    BytesPerLogicalSector         = %lu\r\n"
            L"    BytesPerPhysicalSector        = %lu\r\n"
            L"    BytesOffsetForSectorAlignment = %lu\r\n"
            L"}", alignment.Version, alignment.Size, alignment.BytesPerCacheLine, alignment.BytesOffsetForCacheAlignment,
            alignment.BytesPerLogicalSector, alignment.BytesPerPhysicalSector, alignment.BytesOffsetForSectorAlignment);

        return alignment.BytesPerPhysicalSector;
    }

    virtual uint32_t _read(void* buffer, uint64_t offset, uint32_t size)
    {
        DWORD   read = 0;
        LARGE_INTEGER   pos = { 0 };
        pos.QuadPart = offset;

        if (!SetFilePointerEx(_hDevice, pos, NULL, FILE_BEGIN))
        {
            printLastError(L"SetFilePointerEx Failed!");
            exit(3);
        }

        if (!ReadFile(_hDevice, buffer, size, &read, NULL))
        {
            printLastError(L"ReadFile Failed!");
            exit(4);
        }

        return read;
    }
};
#endif	// _WIN32


std::unique_ptr<BlockDevice> open(wchar_t* path)
{
#ifdef _WIN32
    return std::unique_ptr<BlockDevice>(new WindowsBlockDevice(path));
#else
	// TODO: POSIX version
#endif
}
