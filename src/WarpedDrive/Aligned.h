#pragma once


struct free_delete
{
    void operator()(void* buffer) { free(buffer); }
};


class BlockDevice
{
    uint32_t _cbSector = 0;

protected:

    virtual uint32_t _getSectorSize() = 0;

    virtual uint32_t _read(void* buffer, uint64_t offset, uint32_t size) = 0;

public:

    uint32_t getSectorSize();

    uint32_t read(void* buffer, uint64_t offset, uint32_t size);
};


std::unique_ptr<BlockDevice> open(wchar_t* path);
