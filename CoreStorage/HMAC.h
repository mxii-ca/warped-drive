#pragma once


template <typename Hash>
class HMAC
{
	Hash        _inner;
	Hash        _outer;

protected:
	uint16_t    _bits = 0;
	uint8_t     _chunk = 0;

public:

	HMAC(uint8_t* key, uint32_t cbKey);
	HMAC(const HMAC &copy) : _inner(copy._inner), _outer(copy._outer), _bits(copy._bits), _chunk(copy._chunk) {};

	uint8_t block_size() { return _chunk; };

	uint8_t digest_size() { return (uint8_t)(_bits / 8); };

	virtual void update(const uint8_t* data, uint32_t cbData);

	virtual void finalize(uint8_t* hmac);
};
