#pragma once


class Hash
{

protected:
	uint16_t    _bits = 0;
	uint8_t     _chunk = 0;

public:

	Hash(uint16_t bits, uint8_t chunk) : _bits(bits), _chunk(chunk) {};

	uint8_t block_size() { return _chunk; };

	uint8_t digest_size() { return (uint8_t)(_bits / 8); };

	virtual void update(const uint8_t* data, uint32_t cbData) = 0;

	virtual void finalize(uint8_t* hash) = 0;
};
