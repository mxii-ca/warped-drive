// HMAC.cpp : HMAC
//

#include "stdafx.h"


template <typename Hash>
HMAC<Hash>::HMAC(uint8_t* key, uint32_t cbKey) : _bits(_outer.digest_size() * 8), _chunk(_inner.block_size())
{
	uint8_t block[128] = { 0 };
	uint8_t xored[128] = { 0 };

	if (cbKey <= _chunk)
	{
		memcpy(block, key, cbKey);
	}
	else
	{
		Hash hash;
		hash.update(key, cbKey);
		hash.finalize(block);
	}

	for (uint8_t i = 0; i < _chunk; i++)
	{
		xored[i] = 0x36 ^ block[i];
	}
	_inner.update(xored, _chunk);

	for (uint8_t i = 0; i < _chunk; i++)
	{
		xored[i] = 0x5c ^ block[i];
	}
	_outer.update(xored, _chunk);
}


template <typename Hash>
void HMAC<Hash>::update(const uint8_t* data, uint32_t cbData)
{
	_inner.update(data, cbData);
}


template <typename Hash>
void HMAC<Hash>::finalize(uint8_t* hmac)
{
	uint8_t block[128] = { 0 };

	_inner.finalize(block);
	_outer.update(block, _inner.digest_size());
	_outer.finalize(hmac);
}


template class HMAC<SHA256>;
