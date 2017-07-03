#pragma once


template <typename Hash>
void pbkdf2(uint8_t* password, uint32_t cbPassword, uint8_t* salt, uint32_t cbSalt, uint32_t iterations, uint8_t* key, uint32_t cbKey);
