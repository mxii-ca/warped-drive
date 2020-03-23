// CoreStorage.cpp : Mac OS X Core Storage.
//

#include "stdafx.h"


#define UUID_FORMAT_STR             "%02hhX%02hhX%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX"

#define UUID_FORMAT_WCS             L"%02hhX%02hhX%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX"

#define UUID_FORMAT_VALUE(buffer)   buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], \
                                    buffer[8], buffer[9], buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15]

#define UUID_FORMAT_REF(buffer)     &(buffer[0]), &(buffer[1]), &(buffer[2]), &(buffer[3]), &(buffer[4]), &(buffer[5]), &(buffer[6]), &(buffer[7]), \
                                    &(buffer[8]), &(buffer[9]), &(buffer[10]), &(buffer[11]), &(buffer[12]), &(buffer[13]), &(buffer[14]), &(buffer[15])


#pragma pack(push, 1)

struct CS_BLOCK_HEADER
{
    uint32_t    Checksum;               // CRC-32 of bytes 8-512
    uint32_t    ChecksumSeed;
    uint16_t    Version;
    uint16_t    BlockType;
    uint32_t    SequenceNumber;
    uint64_t    RevisionNumber;
    uint8_t     Unknown_0[8];           // relative block number / parent?
    uint64_t    BlockNumber;
    uint8_t     Unknown_1[8];           // relative block number / first block in a chain?
    uint32_t    HeaderSize;
    uint8_t     Unknown_2[4];           // flags?
    uint8_t     Unknown_3[8];
    uint64_t    BlockSize;
};

struct CS_BLOCK_10_HEADER               // Volume Header
{
    // CS_BLOCK_HEADER
    uint8_t     Unknown_0[16];          // volume resize data?
    char        Signature[2];           // CS
    uint32_t    ChecksumAlgorithm;      // 1 - CRC-32C (Castagnoli polinomial 0x1edc6f41)
    uint16_t    MetadataBlocks;         // max 8?
    uint32_t    BlockSize;
    uint32_t    MetadataSize;
    uint64_t    MetadataBlock[8];
    uint32_t    KeyDataSize;
    uint32_t    EncryptionAlgorithm;    // 2 - AES-XTS
    uint8_t     KeyData[16];
    uint8_t     Unknown_1[112];         // more key data?
    uint8_t     PhysicalVolumeUUID[16];
    uint8_t     GroupVolumeUUID[16];
    uint8_t     Unknown_2[176];         // Unused?
};

struct CS_BLOCK_11_HEADER
{
    // CS_BLOCK_HEADER
    uint32_t    Checksum;               // CRC-32 of bytes?
    uint32_t    ChecksumSeed;
    uint8_t     Unknown_0[4 * 4 + 72 + 4 * 7 + 24];
    uint32_t    VolumeGroupsOffset;
    uint32_t    XmlOffset;
    uint32_t    XmlSize;
    uint32_t    Unknown_1[4 + 4];
    uint64_t    PhysicalBlocks;         // or block number of backup?
    uint8_t     Unknown_3[8];
    //uint8_t    Unknown_4[24 * ?]       // array of unknown values
};

struct CS_VOLUME_GROUPS_DESCRIPTOR
{
    uint8_t     Unknown_0[8];
    uint64_t    EncryptedMetadataSize;  // number of blocks
    uint8_t     Unknown_1[8];
    uint64_t    EncryptedMetadataBlocks;
    uint64_t    EncryptedMetadataBlock[2];
};

struct CS_BLOCK_13_HEADER
{
    // CS_BLOCK_HEADER
    uint8_t     GroupVolumeUUID[16];
};

struct CS_BLOCK_19_HEADER
{
    // CS_BLOCK_HEADER
    uint8_t     Unknown_0[8 * 4 + 4 * 2];
    uint32_t    XmlOffset;
    uint32_t    XmlSize;
    //uint8_t    Unknown_1[2 * 4 + ?]
};

struct CS_BLOCK_1a_HEADER
{
    // CS_BLOCK_HEADER
    uint8_t     Unknown_0[8 + 16 + 8 * 3 + 4 * 2];
    uint32_t    XmlOffset;
    uint32_t    XmlSize;
    //uint8_t    Unknown_1[?]
};

struct CS_BLOCK_505_HEADER
{
    // CS_BLOCK_HEADER
    uint64_t    Blocks;
    uint64_t    FirstBlock;
};

struct CS_PASSPHRASE_WRAPPED_KEK
{
    uint32_t    SaltType;				// 3
    uint32_t    SaltSize;				// 16
    uint8_t     Salt[16];
    uint32_t    KeyType;				// 16
    uint32_t    KeySize;				// 24
    uint8_t     Key[24];
    uint8_t     Unknown_0[4 + 8 + 4 + 72 + 4 * 3 + 12 + 4];
    uint32_t    Iterations;
    //uint8_t    Unknown_1[4 * 3 + 96]
};

struct CS_KEK_WRAPPED_VOLUME_KEY
{
    uint32_t    KeyType;
    uint32_t    KeySize;
    uint8_t     Key[24];
    //uint8_t     Unknown_0[4 + 8 + 4 * 4 + 8 + 4 + 72 + 4 * 3 + 96 + 4];
};

#pragma pack(pop)



// === libfvde ===
// mount_handle_initialize -> libfvde_volume_initialize -> libfvde_metadata_initialize *4
//                                                     \-> libfvde_encrypted_metadata_initialize *2
//                                                     \-> io_handle_initialize
//                                                     \-> keyring_initialize
// mount_handle_set_password -> libfvde_volume_set_utf16/8_password -> CODEPAGE_US_ASCII
// mount_handle_open_input -> libfvde_volume_open_file_io_handle -> volume_open_read -> io_handle_read_volume_header -> checksum_calculate_weak_crc
//                                                                                  \-> metadata_read *4 -> metadata_block_read
//                                                                                                      \-> metadata_read_type_0x0011 -> metadata_read_core_storage_plist
//                                                                                                                                       \-> com.apple.corestorage.lvg.uuid (LogicalVolumeUUID)
//                                                                                  \-> encrypted_metadata_read *2 -> libcaes_tweaked_context_initialize
//                                                                                                                \-> libcaes_tweaked_context_set_keys  KeyData  PhysicalVolumeUUID
//                                                                                                                                                  \-> context_set_key DECRYPT KeyData
//                                                                                                                                                  \-> context_set_key ENCRYPT PhysicalVolumeUUID
//                                                                                                                \-> metadata_block_check_for_empty_block
//                                                                                                                \-> libcaes_crypt_xts      tweak_value = block number [16]    block size = 8192
//                                                                                                                                  \-> crypt_ecb    tweak_value    PhysicalVolumeUUID
//                                                                                  \-> volume_open_read_keys_from_encrypted_metadata -> encrypted_metadata_get_volume_master_key
//                                                                                                                                                         \-> encryption_context_plist_get_passphrase_wrapped_kek
//                                                                                                                                                         \-> password_pbkdf2
//																																						   \-> libfvde_encryption_aes_key_unwrap
//                                                                                                                                   \-> sha256 volume_master_key[16] familyUUID[16] -> volume_tweak_key
//                                                                                                                                   \-> tweaked_context_initialize
//                                                                                                                                   \-> tweaked_context_set_keys  volume_master_key  volume_tweak_key
//                                                                                      


class CoreStorageVolume
{
    uint64_t    _volume_size = 0;

    uint32_t    _block_size = 0;
    uint64_t    _first_block = 0;
    uint64_t    _num_blocks = 0;

    uint8_t     _key_data[16] = { 0 };

    uint8_t     _physical_uuid[16] = { 0 };
    uint8_t     _group_uuid[16] = { 0 };
    uint8_t     _family_uuid[16] = { 0 };
    uint8_t     _logical_uuid[16] = { 0 };

    std::unique_ptr<BlockDevice>    _device;

public:

    CoreStorageVolume(std::unique_ptr<BlockDevice> &device) : _device(std::move(device)) {}

    void readAndParseBlockHeader(uint64_t offset);

    void readAndParseEncryptedHeaders(uint64_t offset, uint32_t size);

    void parseBlockHeader(uint64_t offset, uint8_t* header, uint32_t size, bool encrypted);

    void parseBlock10(uint64_t offset, uint8_t* header, uint32_t size);

    void parseBlock11(uint64_t offset, uint8_t* header, uint32_t size);

    void parseBlock19(uint64_t offset, uint8_t* header, uint32_t size);

    void parseBlock1a(uint64_t offset, uint8_t* header, uint32_t size);

    void parseBlock505(uint64_t offset, uint8_t* header, uint32_t size);
};


void CoreStorageVolume::readAndParseBlockHeader(uint64_t offset)
{
    uint8_t header[sizeof(CS_BLOCK_HEADER)] = { 0 };

    _device->read(header, offset, sizeof(CS_BLOCK_HEADER));
    //xxd(header, offset, sizeof(CS_BLOCK_HEADER));

    parseBlockHeader(offset, header, sizeof(CS_BLOCK_HEADER), false);
}


void CoreStorageVolume::readAndParseEncryptedHeaders(uint64_t offset, uint32_t size)
{
    std::unique_ptr<uint8_t, free_delete>   encrypted((uint8_t*)malloc(size));
    AESCipher                               aes(_key_data, 16, _physical_uuid, 16, false);
    uint8_t                                 value[16] = { 0 };
    uint32_t                                blockNum = 0;
    uint32_t                                index = 0;
    uint32_t                                i = 0;

    _device->read(encrypted.get(), offset, size);
    //xxd(encrypted.get(), offset, size);

    for (index = 0; index < size; index += 8192)
    {
        for (i = 1; i < 8192; i++)
        {
            if (encrypted.get()[index + i] != encrypted.get()[index])
            {
                break;
            }
        }
        if (i < 8192)
        {
            std::unique_ptr<uint8_t, free_delete>   decrypted((uint8_t*)malloc(8192));
            memset(decrypted.get(), 0, 8192);

            memset(value, 0, 16);
            *((uint64_t*)value) = (uint64_t)blockNum;

            aes.xts(encrypted.get() + index, 8192, value, decrypted.get(), 8192);
            //xxd(decrypted.get(), offset, 72);

            parseBlockHeader(offset + index, decrypted.get(), 8192, true);
        }
        else if (0 != encrypted.get()[index])
        {
            break;
        }
        blockNum++;
    }
}


typedef void(CoreStorageVolume::*ParseBlock_f)(uint64_t offset, uint8_t* header, uint32_t size);

void CoreStorageVolume::parseBlockHeader(uint64_t offset, uint8_t* header, uint32_t size, bool encrypted)
{
    CS_BLOCK_HEADER*    blockHeader = (CS_BLOCK_HEADER*)header;
    ParseBlock_f        parser_f = NULL;

    if (size < sizeof(CS_BLOCK_HEADER))
    {
        debug(L"ERROR: parseBlockHeader: size MUST be at least %u bytes.", sizeof(CS_BLOCK_HEADER));
        exit(5);
    }

    if (0 == memcmp(header, (uint8_t*)"LVFwiped", sizeof("LVFwiped") - 1))
    {
        //debug(L"Skipping LVFwiped BLOCK");
        return;
    }

    switch (blockHeader->BlockType)
    {
        // block types 0x0010 and 0x0011 are only valuable when unencrypted
    case 0x0010:
        parser_f = &CoreStorageVolume::parseBlock10;
        if (encrypted) return;
        if (_block_size) return;
        break;
    case 0x0011:
        parser_f = &CoreStorageVolume::parseBlock11;
        if (encrypted) return;
        break;
        // rest of the blocks are only present encrypted
    case 0x0019:
        parser_f = &CoreStorageVolume::parseBlock19;
        break;
    case 0x001a:
        parser_f = &CoreStorageVolume::parseBlock1a;
        if (_volume_size) return;
        break;
    case 0x0505:
        parser_f = &CoreStorageVolume::parseBlock505;
        if (_num_blocks) return;
        break;
    case 0x0012:
        // same xml data as block 0x0011 at offset 40
    case 0x0013:
        // group uuid at offset 0
    case 0x0021:
        // number of blocks at offset -6
    case 0x0016:
    case 0x0017:
    case 0x0018:
    case 0x001c:
    case 0x001d:
    case 0x0022:
    case 0x0024:
    case 0x0105:
    case 0x0205:
    case 0x0305:
    case 0x0405:
    case 0x0605:
        // unknown data
        //debug(L"Skipping BlockType: %04hx", blockHeader->BlockType);
        return;
    default:
        debug(L"ERROR: parseBlockHeader: unknown BlockType: %04hx", blockHeader->BlockType);
        exit(6);
    }

    debug(L"CS_BLOCK_HEADER {\r\n"
        L"  Checksum            = %08x\r\n"
        L"  ChecksumSeed        = %08x\r\n"
        L"  Version             = %u\r\n"
        L"  BlockType           = %04hx\r\n"
        L"  SequenceNumber      = %u\r\n"
        L"  RevisionNumber      = %llu\r\n"
        L"  Unknown\r\n"
        L"  BlockNumber         = %llu\r\n"
        L"  Unknown\r\n"
        L"  HeaderSize          = %u\r\n"
        L"  Unknown\r\n"
        L"  BlockSize           = %llu\r\n"
        L"}", blockHeader->Checksum, blockHeader->ChecksumSeed, blockHeader->Version,
        blockHeader->BlockType, blockHeader->SequenceNumber, blockHeader->RevisionNumber,
        blockHeader->BlockNumber, blockHeader->HeaderSize, blockHeader->BlockSize);

    if (size >= blockHeader->HeaderSize)
    {
        (this->*parser_f)(offset, header + sizeof(CS_BLOCK_HEADER), size - sizeof(CS_BLOCK_HEADER));
    }
    else if (!encrypted)
    {
        std::unique_ptr<uint8_t, free_delete>  extendedHeader((uint8_t*)malloc(blockHeader->HeaderSize - sizeof(CS_BLOCK_HEADER)));

        if (size > sizeof(CS_BLOCK_HEADER))
        {
            memcpy(extendedHeader.get(), header + sizeof(CS_BLOCK_HEADER), size - sizeof(CS_BLOCK_HEADER));
        }

        _device->read(extendedHeader.get() + size - sizeof(CS_BLOCK_HEADER), offset + size, blockHeader->HeaderSize - size);
        //xxd(extendedHeader.get(), offset + sizeof(CS_BLOCK_HEADER), blockHeader->HeaderSize - sizeof(CS_BLOCK_HEADER));

        (this->*parser_f)(offset, extendedHeader.get(), blockHeader->HeaderSize - sizeof(CS_BLOCK_HEADER));
    }
    else
    {
        debug(L"ERROR: parseBlockHeader: Failed to parse Encrypted BlockType: %04hx", blockHeader->BlockType);
        exit(7);
    }
}


void CoreStorageVolume::parseBlock10(uint64_t offset, uint8_t* header, uint32_t size)
{
    CS_BLOCK_10_HEADER* volumeHeader = (CS_BLOCK_10_HEADER*)header;
    uint32_t            metadataNum = 0;
    uint64_t            metadataOffset = 0;

    if (size < sizeof(CS_BLOCK_10_HEADER))
    {
        debug(L"ERROR: parseBlock10: size MUST be at least %u bytes.", sizeof(CS_BLOCK_10_HEADER));
        exit(5);
    }

    debug(L"CS_BLOCK_10_HEADER {\r\n"
        L"  Unknown\r\n"
        L"  Signature           = %c %c\r\n"
        L"  ChecksumAlgorithm   = %u\r\n"
        L"  MetadataBlocks      = %u\r\n"
        L"  BlockSize           = %u\r\n"
        L"  MetadataSize        = %u\r\n"
        L"  MetadataBlock1      = %llu\r\n"
        L"  MetadataBlock2      = %llu\r\n"
        L"  MetadataBlock3      = %llu\r\n"
        L"  MetadataBlock4      = %llu\r\n"
        L"  MetadataBlock5      = %llu\r\n"
        L"  MetadataBlock6      = %llu\r\n"
        L"  MetadataBlock7      = %llu\r\n"
        L"  MetadataBlock8      = %llu\r\n"
        L"  KeyDataSize         = %u\r\n"
        L"  EncryptionAlgorithm = %u\r\n"
        L"  KeyData\r\n"
        L"  PhysicalVolumeUUID  = " UUID_FORMAT_WCS L"\r\n"
        L"  GroupVolumeUUID     = " UUID_FORMAT_WCS L"\r\n"
        L"  Unknown\r\n"
        L"}", volumeHeader->Signature[0], volumeHeader->Signature[1], volumeHeader->ChecksumAlgorithm,
        volumeHeader->MetadataBlocks, volumeHeader->BlockSize, volumeHeader->MetadataSize,
        volumeHeader->MetadataBlock[0], volumeHeader->MetadataBlock[1], volumeHeader->MetadataBlock[2],
        volumeHeader->MetadataBlock[3], volumeHeader->MetadataBlock[4], volumeHeader->MetadataBlock[5],
        volumeHeader->MetadataBlock[6], volumeHeader->MetadataBlock[7], volumeHeader->KeyDataSize, volumeHeader->EncryptionAlgorithm,
        UUID_FORMAT_VALUE(volumeHeader->PhysicalVolumeUUID), UUID_FORMAT_VALUE(volumeHeader->GroupVolumeUUID));

    // only bother populating volume information once
    if (!_block_size)
    {
        _block_size = volumeHeader->BlockSize;

        memcpy(_key_data, volumeHeader->KeyData, 16);
        memcpy(_physical_uuid, volumeHeader->PhysicalVolumeUUID, 16);
        memcpy(_group_uuid, volumeHeader->GroupVolumeUUID, 16);

        for (metadataNum = volumeHeader->MetadataBlocks; metadataNum > 0; metadataNum--)
        {
            metadataOffset = volumeHeader->MetadataBlock[metadataNum - 1] * _block_size;

            debug(L"Metadata %u Offset: %llu", metadataNum, metadataOffset);

            // subsequent metadata blocks should be backups of the first
            if (1 == metadataNum) {
                readAndParseBlockHeader(metadataOffset);
            }
        }
    }
}


void CoreStorageVolume::parseBlock11(uint64_t offset, uint8_t* header, uint32_t size)
{
    CS_BLOCK_11_HEADER* metadataHeader = (CS_BLOCK_11_HEADER*)header;
    uint32_t            metadataSize = 0;
    uint32_t            metadataNum = 0;
    uint64_t            metadataOffset = 0;

    if (size < sizeof(CS_BLOCK_11_HEADER))
    {
        debug(L"ERROR: parseBlock11: size MUST be at least %u bytes.", sizeof(CS_BLOCK_11_HEADER));
        exit(5);
    }

    debug(L"CS_BLOCK_11_HEADER {\r\n"
        L"  Checksum            = %08x\r\n"
        L"  ChecksumSeed        = %08x\r\n"
        L"  Unknown\r\n"
        L"  VolumeGroupsOffset  = %u\r\n"
        L"  XmlOffset           = %u\r\n"
        L"  XmlSize             = %u\r\n"
        L"  Unknown\r\n"
        L"  PhysicalBlocks      = %llu\r\n"
        L"  Unknown\r\n"
        L"}", metadataHeader->Checksum, metadataHeader->ChecksumSeed, metadataHeader->VolumeGroupsOffset,
        metadataHeader->XmlOffset, metadataHeader->XmlSize, metadataHeader->PhysicalBlocks);

    {
        uint8_t                         descriptor[sizeof(CS_VOLUME_GROUPS_DESCRIPTOR)] = { 0 };
        CS_VOLUME_GROUPS_DESCRIPTOR*    volumeDescriptor = (CS_VOLUME_GROUPS_DESCRIPTOR*)descriptor;

        _device->read(descriptor, offset + metadataHeader->VolumeGroupsOffset, sizeof(CS_VOLUME_GROUPS_DESCRIPTOR));
        //xxd(descriptor, offset + metadataHeader->VolumeGroupsOffset, sizeof(CS_VOLUME_GROUPS_DESCRIPTOR));
        
        debug(L"CS_VOLUME_GROUPS_DESCRIPTOR {\r\n"
            L"  Unknown\r\n"
            L"  EncryptedMetadataSize   = %llu\r\n"
            L"  Unknown\r\n"
            L"  EncryptedMetadataBlocks = %llu\r\n"
            L"  EncryptedMetadataBlock1 = %llu\r\n"
            L"  EncryptedMetadataBlock2 = %llu\r\n"
            L"}", volumeDescriptor->EncryptedMetadataSize, volumeDescriptor->EncryptedMetadataBlocks,
            volumeDescriptor->EncryptedMetadataBlock[0], volumeDescriptor->EncryptedMetadataBlock[1]);

        if (metadataHeader->XmlSize)
        {
            std::unique_ptr<uint8_t, free_delete>  xml((uint8_t*)malloc(metadataHeader->XmlSize));

            _device->read(xml.get(), offset + metadataHeader->XmlOffset, metadataHeader->XmlSize);
            //xxd(xml.get(), offset + metadataHeader->XmlOffset, metadataHeader->XmlSize);
            {
                PlistEntry plist((char*)xml.get());
                plist.debug_print(0);
            }
        }

        if (volumeDescriptor->EncryptedMetadataSize)
        {
            metadataSize = (uint32_t)(volumeDescriptor->EncryptedMetadataSize * _block_size);

            debug(L"Metadata Size: %u", metadataSize);

            for (metadataNum = (uint32_t)volumeDescriptor->EncryptedMetadataBlocks; metadataNum > 0; metadataNum--)
            {
                metadataOffset = volumeDescriptor->EncryptedMetadataBlock[metadataNum - 1] * _block_size;

                debug(L"Metadata %u Offset: %llu", metadataNum, metadataOffset);

                // subsequent metadata blocks should be backups of the first
                if (1 == metadataNum) {
                    readAndParseEncryptedHeaders(metadataOffset, metadataSize);
                }
            }
        }
    }
}


void CoreStorageVolume::parseBlock19(uint64_t offset, uint8_t* header, uint32_t size)
{
    CS_BLOCK_19_HEADER* metadataHeader = (CS_BLOCK_19_HEADER*)header;

    if (size < sizeof(CS_BLOCK_19_HEADER))
    {
        debug(L"ERROR: parseBlock19: size MUST be at least %u bytes.", sizeof(CS_BLOCK_19_HEADER));
        exit(5);
    }

    debug(L"CS_BLOCK_19_HEADER {\r\n"
        L"  Unknown\r\n"
        L"  XmlOffset           = %u\r\n"
        L"  XmlSize             = %u\r\n"
        L"  Unknown\r\n"
        L"}", metadataHeader->XmlOffset, metadataHeader->XmlSize);

    if (metadataHeader->XmlSize)
    {
        uint8_t*    xml = header - sizeof(CS_BLOCK_HEADER) + metadataHeader->XmlOffset;
        PlistEntry  plist((char*)xml);
        PlistEntry* context = NULL;

        //xxd(xml, offset + metadataHeader->XmlOffset, metadataHeader->XmlSize);
        plist.debug_print(0);

        if (0 != strcmp(plist.type, "dict"))
        {
            debug(L"ERROR: parseBlock19: invalid xml data.");
            exit(7);
        }

        context = plist.get("com.apple.corestorage.lvf.encryption.context");
        if (context && (0 == strcmp(context->type, "dict")))
        {
            PlistEntry* keys = NULL;
            
            keys = context->get("CryptoUsers");
            if (keys && (0 == strcmp(keys->type, "array")))
            {
                for (uint32_t i = 0; i < keys->children.size(); i++)
                {
                    PlistEntry* kekBase64 = keys->children.at(i).get("PassphraseWrappedKEKStruct");
                    if (kekBase64 && kekBase64->value && (0 == strcmp(kekBase64->type, "data")))
                    {
                        std::unique_ptr<uint8_t, free_delete>   kekData((uint8_t*)malloc(284));
                        CS_PASSPHRASE_WRAPPED_KEK*              kek = (CS_PASSPHRASE_WRAPPED_KEK*)kekData.get();

                        base64_decode(kekBase64->value, strlen(kekBase64->value), kekData.get(), 284);

                        debug(L"CS_PASSPHRASE_WRAPPED_KEK {\r\n"
                            L"  SaltType    = %u\r\n"
                            L"  SaltSize    = %u\r\n"
                            L"  Salt\r\n"
                            L"  KeyType     = %u\r\n"
                            L"  KeySize     = %u\r\n"
                            L"  Key\r\n"
                            L"  Unknown\r\n"
                            L"  Iterations  = %u\r\n"
                            L"  Unknown\r\n"
                            L"}", kek->SaltType, kek->SaltSize, kek->KeyType, kek->KeySize, kek->Iterations);

						{
							uint8_t passKey[16] = { 0 };
							uint8_t kekKey[16] = { 0 };
							std::unique_ptr<uint8_t, free_delete> passwd = getPassword(L"Password:");

							pbkdf2<SHA256>(passwd.get(), strlen((char*)passwd.get()), kek->Salt, kek->SaltSize, kek->Iterations, passKey, sizeof(passKey));

							debug(L"PASSPHRASE_KEY { %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X }", UUID_FORMAT_VALUE(passKey));

							aes_unwrap(passKey, sizeof(passKey), kek->Key, kek->KeySize, kekKey, sizeof(kekKey));

							debug(L"KEK_KEY { %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X }", UUID_FORMAT_VALUE((kekKey)));
						}
                    }
                }
            }

            keys = context->get("WrappedVolumeKeys");
            if (keys && (0 == strcmp(keys->type, "array")))
            {
                for (uint32_t i = 0; i < keys->children.size(); i++)
                {
                    PlistEntry* kekBase64 = keys->children.at(i).get("KEKWrappedVolumeKeyStruct");
                    if (kekBase64 && kekBase64->value && kekBase64->value[0] && (0 == strcmp(kekBase64->type, "data")))
                    {
                        std::unique_ptr<uint8_t, free_delete>   kekData((uint8_t*)malloc(256));
                        CS_KEK_WRAPPED_VOLUME_KEY*              kek = (CS_KEK_WRAPPED_VOLUME_KEY*)kekData.get();

                        base64_decode(kekBase64->value, strlen(kekBase64->value), kekData.get(), 284);

                        debug(L"CS_KEK_WRAPPED_VOLUME_KEY {\r\n"
                            L"  KeyType     = %u\r\n"
                            L"  KeySize     = %u\r\n"
                            L"  Key\r\n"
                            L"  Unknown\r\n"
                            L"}", kek->KeyType, kek->KeySize);
                    }
                }
            }
        }
    }
}


void CoreStorageVolume::parseBlock1a(uint64_t offset, uint8_t* header, uint32_t size)
{
    CS_BLOCK_1a_HEADER* metadataHeader = (CS_BLOCK_1a_HEADER*)header;

    if (size < sizeof(CS_BLOCK_1a_HEADER))
    {
        debug(L"ERROR: parseBlock1a: size MUST be at least %u bytes.", sizeof(CS_BLOCK_1a_HEADER));
        exit(5);
    }

    debug(L"CS_BLOCK_1a_HEADER {\r\n"
        L"  Unknown\r\n"
        L"  XmlOffset           = %u\r\n"
        L"  XmlSize             = %u\r\n"
        L"  Unknown\r\n"
        L"}", metadataHeader->XmlOffset, metadataHeader->XmlSize);

    // only bother populating volume information once
    if (!_volume_size && metadataHeader->XmlSize)
    {
        uint8_t*    xml = header - sizeof(CS_BLOCK_HEADER) + metadataHeader->XmlOffset;
        PlistEntry  plist((char*)xml);
        PlistEntry* entry = NULL;

        //xxd(xml, offset + metadataHeader->XmlOffset, metadataHeader->XmlSize);
        plist.debug_print(0);

        if (0 != strcmp(plist.type, "dict"))
        {
            debug(L"ERROR: parseBlock1a: invalid xml data.");
            exit(7);
        }

        entry = plist.get("com.apple.corestorage.lv.familyUUID");
        if (entry && entry->value && (0 == strcmp(entry->type, "string")))
        {
            if (16 != sscanf_s(entry->value, UUID_FORMAT_STR, UUID_FORMAT_REF(_family_uuid)))
            {
                debug(L"ERROR: parseBlock1a: invalid xml data.");
                exit(7);
            }
        }
        else
        {
            debug(L"ERROR: parseBlock1a: invalid xml data.");
            exit(7);
        }

        entry = plist.get("com.apple.corestorage.lv.uuid");
        if (entry && entry->value && (0 == strcmp(entry->type, "string")))
        {
            if (16 != sscanf_s(entry->value, UUID_FORMAT_STR, UUID_FORMAT_REF(_logical_uuid)))
            {
                debug(L"ERROR: parseBlock1a: invalid xml data.");
                exit(7);
            }
        }
        else
        {
            debug(L"ERROR: parseBlock1a: invalid xml data.");
            exit(7);
        }

        entry = plist.get("com.apple.corestorage.lv.size");
        if (entry && entry->value && (0 == strcmp(entry->type, "integer")))
        {
            if (1 != sscanf_s(entry->value, "0x%llx", &_volume_size))
            {
                debug(L"ERROR: parseBlock1a: invalid xml data.");
                exit(7);
            }
        }
    }
}


void CoreStorageVolume::parseBlock505(uint64_t offset, uint8_t* header, uint32_t size)
{
    CS_BLOCK_505_HEADER*    volumeHeader = (CS_BLOCK_505_HEADER*)header;

    if (size < sizeof(CS_BLOCK_505_HEADER))
    {
        debug(L"ERROR: parseBlock505: size MUST be at least %u bytes.", sizeof(CS_BLOCK_505_HEADER));
        exit(5);
    }

    debug(L"CS_BLOCK_505_HEADER {\r\n"
        L"  Blocks      = %llu\r\n"
        L"  FirstBlock  = %llu\r\n"
        L"}", volumeHeader->Blocks, volumeHeader->FirstBlock);

    // only bother populating volume information once
    // only the first 505 contains the total number of blocks
    // subsequent 505 decreases the number of blocks
    if (!_num_blocks)
    {
        _num_blocks = volumeHeader->Blocks;
        _first_block = volumeHeader->FirstBlock;
    }
}


void parseCoreStorage(std::unique_ptr<BlockDevice> &device, uint8_t header[512])
{
    CoreStorageVolume   volume(device);
    
    volume.parseBlockHeader(0, header, 512, false);
}
