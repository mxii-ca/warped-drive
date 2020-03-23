// PBKDF.cpp : PBKDF
//

#include "stdafx.h"


template <typename Hash>
void pbkdf2(uint8_t* password, uint32_t cbPassword, uint8_t* salt, uint32_t cbSalt, uint32_t iterations, uint8_t* key, uint32_t cbKey)
{
	HMAC<Hash> hmac(password, cbPassword);
	uint32_t size = hmac.digest_size();
	uint32_t block = 1;

	while (cbKey > 0)
	{
		HMAC<Hash> init(hmac);
		uint8_t hash[128] = { 0 };

		hash[0] = block >> 24;
		hash[1] = block >> 16;
		hash[2] = block >> 8;
		hash[3] = block;

		init.update(salt, cbSalt);
		init.update(hash, 4);
		init.finalize(hash);

		memcpy(key, hash, min(size, cbKey));

		for (uint32_t j = 0; j < iterations - 1; j++)
		{
			HMAC<Hash> iter(hmac);

			iter.update(hash, size);
			iter.finalize(hash);

			for (uint32_t k = 0; k < min(size, cbKey); k++)
			{
				key[k] ^= hash[k];
			}
		}

		block++;
		key += min(size, cbKey);
		cbKey -= min(size, cbKey);
	}
}


template void pbkdf2<SHA256>(uint8_t* password, uint32_t cbPassword, uint8_t* salt, uint32_t cbSalt, uint32_t iterations, uint8_t* key, uint32_t cbKey);
