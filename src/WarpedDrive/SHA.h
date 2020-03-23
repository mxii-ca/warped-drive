#pragma once


class SHA2 : public Hash
{
    uint8_t     _width = 0;

    uint64_t    _hash[8] = { 0 };

    uint64_t    _processed = 0;
    uint8_t     _partial = 0;
    uint8_t     _buffer[128] = { 0 };

protected:

	void _update(const uint8_t* data);

public:

    SHA2(uint16_t bits);
	SHA2(const SHA2 &copy);

	void update(const uint8_t* data, uint32_t cbData);

    void finalize(uint8_t* hash);
};


class SHA256 : public SHA2
{
public:
	SHA256() : SHA2(256) {};
};
