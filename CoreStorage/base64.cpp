// base64.cpp : base64 Encoding
//

#include "stdafx.h"


static char BASE64_FORWARD[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

static uint8_t BASE64_REVERSE[128] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0,
    // + == 43, / == 47, 0 == 48, 9 == 57
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62,  0,  0,  0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  0, 0, 0, 0, 0, 0,
    // A == 65, Z == 90
    0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0,
    // a == 97, z == 122
    0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0
};


uint32_t base64_encode(uint8_t* data, uint32_t cbData, char* encoded, uint32_t cbEncoded)
{
    uint32_t    j = 0;

    for (uint32_t i = 0; i < cbData && j < cbEncoded; i += 3)
    {
        encoded[j++] = BASE64_FORWARD[data[i] >> 2];
        if ((i + 1) < cbData)
        {
            encoded[j++] = BASE64_FORWARD[((data[i] & 0x03) << 4) | (data[i + 1] >> 4)];
            if ((i + 2) < cbData)
            {
                encoded[j++] = BASE64_FORWARD[((data[i + 1] & 0x0F) << 2) | (data[i + 2] >> 6)];
                encoded[j++] = BASE64_FORWARD[data[i + 2] & 0x3F];
            }
            else
            {
                encoded[j++] = BASE64_FORWARD[(data[i + 1] & 0x0F) << 2];
                encoded[j++] = '=';
            }
        }
        else
        {
            encoded[j++] = BASE64_FORWARD[(data[i] & 0x03) << 4];
            encoded[j++] = '=';
            encoded[j++] = '=';
        }
    }

    return j;
}


uint32_t base64_decode(char* data, uint32_t cbData, uint8_t* decoded, uint32_t cbDecoded)
{
    uint32_t    j = 0;

    while ('=' == data[cbData - 1])
    {
        cbData--;
    }

    for (uint32_t i = 0; i < cbData && j < cbDecoded; i += 4)
    {
        decoded[j++] = (BASE64_REVERSE[data[i]] << 2) | (BASE64_REVERSE[data[i + 1]] >> 4);
        if (i + 2 < cbData)
        {
            decoded[j++] = (BASE64_REVERSE[data[i + 1]] << 4) | (BASE64_REVERSE[data[i + 2]] >> 2);
            if (i + 3 < cbData)
            {
                decoded[j++] = (BASE64_REVERSE[data[i + 2]] << 6) | BASE64_REVERSE[data[i + 3]];
            }
        }
    }

    return j;
}
