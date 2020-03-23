// SHA.cpp : SHA
//

#include "stdafx.h"


#define ROTATE_RIGHT(value, width, shift)   ((value >> shift) | (value << (width - shift)))


// the fractional parts of the square roots of the first 16 primes 2..53
static uint64_t PRIME_SQUARE_ROOTS[16] = {
    0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1, 0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
    0xcbbb9d5dc1059ed8, 0x629a292a367cd507, 0x9159015a3070dd17, 0x152fecd8f70e5939, 0x67332667ffc00b31, 0x8eb44a8768581511, 0xdb0c2e0d64f98fa7, 0x47b5481dbefa4fa4
};

// the fractional parts of the cube roots of the first 80 primes 2..409
static uint64_t PRIME_CUBE_ROOTS[80] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc, 0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2, 0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65, 0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df, 0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec, 0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
    0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};


SHA2::SHA2(uint16_t bits) : Hash(bits, ((_bits / 4) + 32) & 0xC0), _width(((_bits / 8) + 16) & 0xE0)
{
    if (224 == bits)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            _hash[i] = PRIME_SQUARE_ROOTS[i + 8] & 0x00000000FFFFFFFF;
        }
    }
    else if (256 == bits)
    {
        for (uint8_t i = 0; i < 8; i++)
        {
            _hash[i] = PRIME_SQUARE_ROOTS[i] >> 32;
        }
    }
    else if (384 == bits)
    {
        memcpy(_hash, PRIME_SQUARE_ROOTS + 8, sizeof(uint64_t) * 8);
    }
    else if (512 == bits)
    {
        memcpy(_hash, PRIME_SQUARE_ROOTS, sizeof(uint64_t) * 8);
    }
}


SHA2::SHA2(const SHA2 &copy) : Hash(copy._bits, copy._chunk), _width(copy._width), _processed(copy._processed), _partial(copy._partial) {
	memcpy(_hash, copy._hash, sizeof(_hash));
	memcpy(_buffer, copy._buffer, sizeof(_buffer));
};


void SHA2::_update(const uint8_t* data)
{
    uint64_t    values[80] = { 0 };
    uint64_t    hash[8] = { 0 };
    uint8_t     rounds = 80;

    if (384 > _bits)
    {
        rounds = 64;
    }

    for (uint8_t i = 0; i < 16; i++)
    {
        for (uint8_t j = _width; j > 0; j -= 8)
        {
            values[i] |= ((uint64_t)*(data++)) << (j - 8);
        }
    }

    for (uint8_t i = 16; i < rounds; i++)
    {
        uint64_t s0 = 0;
        uint64_t s1 = 0;

        if (384 > _bits)
        {
            s0 = ROTATE_RIGHT(values[i - 15], _width, 7) ^ ROTATE_RIGHT(values[i - 15], _width, 18) ^ (values[i - 15] >> 3);
            s1 = ROTATE_RIGHT(values[i - 2], _width, 17) ^ ROTATE_RIGHT(values[i - 2], _width, 19) ^ (values[i - 2] >> 10);
        }
        else
        {
            s0 = ROTATE_RIGHT(values[i - 15], _width, 1) ^ ROTATE_RIGHT(values[i - 15], _width, 8) ^ (values[i - 15] >> 7);
            s1 = ROTATE_RIGHT(values[i - 2], _width, 19) ^ ROTATE_RIGHT(values[i - 2], _width, 61) ^ (values[i - 2] >> 6);
        }

        values[i] = values[i - 16] + s0 + values[i - 7] + s1;

        if (384 > _bits)
        {
            values[i] &= 0x00000000FFFFFFFF;
        }
    }

    memcpy(hash, _hash, sizeof(hash));

    for (uint8_t i = 0; i < rounds; i++)
    {
        uint64_t s0 = 0;
        uint64_t s1 = 0;
        uint64_t t1 = 0;
        uint64_t t2 = 0;

        if (384 > _bits)
        {
            s0 = ROTATE_RIGHT(hash[0], _width, 2) ^ ROTATE_RIGHT(hash[0], _width, 13) ^ ROTATE_RIGHT(hash[0], _width, 22);
            s1 = ROTATE_RIGHT(hash[4], _width, 6) ^ ROTATE_RIGHT(hash[4], _width, 11) ^ ROTATE_RIGHT(hash[4], _width, 25);
            t1 = PRIME_CUBE_ROOTS[i] >> 32;
        }
        else
        {
            s0 = ROTATE_RIGHT(hash[0], _width, 28) ^ ROTATE_RIGHT(hash[0], _width, 34) ^ ROTATE_RIGHT(hash[0], _width, 39);
            s1 = ROTATE_RIGHT(hash[4], _width, 14) ^ ROTATE_RIGHT(hash[4], _width, 18) ^ ROTATE_RIGHT(hash[4], _width, 41);
            t1 = PRIME_CUBE_ROOTS[i];
        }

        t1 += s1 + hash[7] + values[i] + ((hash[4] & hash[5]) ^ (~(hash[4]) & hash[6]));
        t2 = s0 + ((hash[0] & hash[1]) ^ (hash[0] & hash[2]) ^ (hash[1] & hash[2]));

        for (uint8_t j = 7; j > 0; j--)
        {
            hash[j] = hash[j - 1];
        }
        hash[0] = t1 + t2;
        hash[4] += t1;

        if (384 > _bits)
        {
            hash[0] &= 0x00000000FFFFFFFF;
            hash[4] &= 0x00000000FFFFFFFF;
        }
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        _hash[i] += hash[i];

        if (384 > _bits)
        {
            _hash[i] &= 0x00000000FFFFFFFF;
        }
    }
}


void SHA2::update(const uint8_t* data, uint32_t cbData)
{
    if (_partial)
    {
        if (cbData >= (uint32_t)(_chunk - _partial))
        {
            memcpy(_buffer + _partial, data, _chunk - _partial);
            _update(_buffer);
            _processed += _chunk - _partial;
            data += _chunk - _partial;
            cbData -= _chunk - _partial;
            _partial = 0;
        }
        else
        {
            memcpy(_buffer + _partial, data, cbData);
            _partial += cbData;
            cbData = 0;
        }
    }

    while (_chunk <= cbData)
    {
        _update(data);
        _processed += _chunk;
        data += _chunk;
        cbData -= _chunk;
    }

    if (cbData)
    {
        memcpy(_buffer, data, cbData);
        _partial = cbData;
    }
}


void SHA2::finalize(uint8_t* hash)
{
    _processed += _partial;
    _processed *= 8;
	
    _buffer[_partial++] = 0x80;
    memset(_buffer + _partial, 0, _chunk - _partial);

    if (_partial > (_chunk - 8))
    {
        _update(_buffer);
        memset(_buffer, 0, _chunk);
    }

    for (uint8_t j = 8; j > 0; j--)
    {
        _buffer[_chunk - j] = (uint8_t)(_processed >> ((j - 1) * 8));
    }
    _update(_buffer);

    for (uint8_t i = 0; i < (_bits / _width); i++)
    {
        for (uint8_t j = _width; j > 0; j -= 8)
        {
            *(hash++) = (uint8_t)(_hash[i] >> (j - 8));
        }
    }
}
