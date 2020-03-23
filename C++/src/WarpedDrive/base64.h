#pragma once


#define BASE64_ENCODE_SIZE(cbData)      ((cbData * 4) / 3)

#define BASE64_DECODE_SIZE(cbData)      ((cbData * 3) / 4)

uint32_t base64_encode(uint8_t* data, uint32_t cbData, char* encoded, uint32_t cbEncoded);

uint32_t base64_decode(char* data, uint32_t cbData, uint8_t* decoded, uint32_t cbDecoded);
