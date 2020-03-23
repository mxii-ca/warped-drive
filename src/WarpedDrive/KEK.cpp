// KEK.cpp : AES Wrapped Key
//

#include "stdafx.h"


void aes_unwrap(uint8_t* key, uint32_t cbKey, uint8_t* encrypted, uint32_t cbEncrypted, uint8_t* decrypted, uint32_t cbDecrypted)
{
	AESCipher aes(key, cbKey, false);
	uint8_t block[16] = { 0 };
	uint32_t n = min(cbDecrypted, cbEncrypted - 8) / 8;

	memcpy(block, encrypted, 8);
	memcpy(decrypted, encrypted + 8, n * 8);

	for (int8_t j = 5; j >= 0; j--)
	{
		for (uint32_t i = n; i > 0; i--)
		{
			uint64_t t = n * j + i;

			for (uint8_t k = 0; k < 8; k++)
			{
				block[k] ^= (uint8_t)(t >> ((7 - k) * 8));
			}
			memcpy(block + 8, decrypted + ((i - 1) * 8), 8);

			aes.ecb(block, sizeof(block), block, sizeof(block));
			memcpy(decrypted + ((i - 1) * 8), block + 8, 8);
		}
	}

	debug(L"INITIALIZATION_VECTOR { %02X %02X %02X %02X %02X %02X %02X %02X }", block[0], block[1], block[2], block[3], block[4], block[5], block[6], block[7]);
}
