#pragma once


typedef union aesint_s
{
    uint32_t    l;
    uint8_t     b[4];

    aesint_s() : l(0) {};
    aesint_s(const uint32_t i) : l(i) {};
    aesint_s(const uint8_t b0, const uint8_t b1, const uint8_t b2, const uint8_t b3) : l(0) { b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3; };

    operator uint32_t() { return l; }

    aesint_s& operator=(const uint32_t o) { l = o; return *this; };

    aesint_s& operator++() { l++; return *this; }
    aesint_s  operator++(int) { aesint_s old(*this); operator++(); return old; }
    aesint_s& operator--() { l--; return *this; }
    aesint_s  operator--(int) { aesint_s old(*this); operator--(); return old; }

    aesint_s& operator+=(const uint32_t& r) { l += r; return *this; }
    aesint_s& operator-=(const uint32_t& r) { l -= r; return *this; }
    aesint_s& operator*=(const uint32_t& r) { l *= r; return *this; }
    aesint_s& operator/=(const uint32_t& r) { l /= r; return *this; }
    aesint_s& operator%=(const uint32_t& r) { l %= r; return *this; }
    aesint_s& operator&=(const uint32_t& r) { l &= r; return *this; }
    aesint_s& operator|=(const uint32_t& r) { l |= r; return *this; }
    aesint_s& operator^=(const uint32_t& r) { l ^= r; return *this; }
    aesint_s& operator<<=(const uint32_t& r) { l <<= r; return *this; }
    aesint_s& operator>>=(const uint32_t& r) { l >>= r; return *this; }

    uint8_t& operator[](std::size_t i) { return b[i]; }
    const uint8_t& operator[](std::size_t i) const { return b[i]; }

    aesint_s rotate() { return aesint_s(b[1], b[2], b[3], b[0]); }
} aesint_t;


class AESCipher
{
    bool                        _encrypt;
    uint8_t                     _cycles;
    aesint_t                    _round_key[60] = { 0 };
    std::unique_ptr<AESCipher>  _tweak;

protected:

    void _init_encrypt(uint8_t* key, uint32_t cbKey);

    void _init_decrypt(uint8_t* key, uint32_t cbKey);

public:

    AESCipher(uint8_t* key, uint32_t cbKey, bool encrypt);

    AESCipher(uint8_t* key, uint32_t cbKey, uint8_t* tweakKey, uint32_t cbTweakKey, bool encrypt);

    uint32_t ecb(const uint8_t* inputData, uint32_t cbInput, uint8_t* outputData, uint32_t cbOutput);

    uint32_t xts(const uint8_t* inputData, uint32_t cbInput, const uint8_t tweakData[16], uint8_t* outputData, uint32_t cbOutput);
};
