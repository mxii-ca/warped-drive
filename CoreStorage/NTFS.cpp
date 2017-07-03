// NTFS.cpp : Windows NTFS.
//

#include "stdafx.h"


#pragma pack(push, 1)

struct BIOS_PARAMETER_BLOCK
{
    // --- DOS 2.0 ---
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;   // 0 for NTFS
    uint8_t NumberOfFATs;       // 0 for NTFS
    uint16_t RootDirectories;   // 0 for NTFS, FAT32
    uint16_t TotalSectors;      // 0 for NTFS, FAT32
    uint8_t MediaDescriptor;
    uint16_t SectorsPerFAT;     // 0 for NTFS, FAT32
                                // --- DOS 3.0 ---
    uint16_t SectorsPerTrack;
    uint16_t NumberOfHeads;
    union
    {
        struct
        {
            uint16_t HiddenSectors;
            // --- DOS 3.2 ---
            uint16_t TotalAndHiddenSectors;
        };
        // --- DOS 3.31 ---
        struct
        {
            uint32_t HiddenSectors32;
            uint32_t TotalSectors32;    // 0 for NTFS
        };
    };
};

struct EXTENDED_BIOS_PARAMETER_BLOCK
{
    // BIOS_PARAMETER_BLOCK
    // --- DOS 4.0 ---
    uint8_t DriveNumber;
    struct
    {
        uint8_t VolumeDirty : 1;
        uint8_t SurfaceScan : 1;
        uint8_t : 6;                // 0
    } ChkdskFlags;
    uint8_t ExtendedParameters;
    // ExtendedParameters == 0x28 or 0x29
    uint8_t VolumeID[4];            // xxxx-xxxx
                                    // ExtendedParameters == 0x29
    char VolumeLabel[11];
    char FilesystemType[8];
};

struct FAT32_EXTENDED_BIOS_PARAMETER_BLOCK
{
    // BIOS_PARAMETER_BLOCK
    // --- DOS 7.1 ---
    uint32_t SectorsPerFAT32;
    struct {
        uint16_t ActiveFATs : 4;
        uint16_t : 3;               // Reserved
        uint16_t AllFATs : 1;
        uint16_t : 8;
    } MirrorFlags;
    uint8_t Version[2];             // 0.0
    uint32_t RootLocation;          // Physical Cluster Number
    uint16_t FSInformationLocation; // Logical Sector Number
    uint16_t BackupLocation;        // Logical Sector Number, 0xFFFF if None
    uint8_t Reserved[12];
    // EXTENDED_BIOS_PARAMETER_BLOCK
};

struct NTFS_EXTENDED_BIOS_PARAMETER_BLOCK
{
    // BIOS_PARAMETER_BLOCK
    uint32_t SectorsPerFAT32;       // 0 for NTFS
    uint64_t TotalSectors64;
    uint64_t MFTLocation;           // Logical Cluster Number
    uint64_t BackupMFTLocation;     // Logical Cluster Number
    int8_t ClustersPerMFTRecord;    // Positive: Number of Clusters, Negative: 2^|x| Bytes
    uint8_t Unused_0[3];
    int8_t ClustersPerIndexBuffer;  // Positive: Number of Clusters, Negative: 2^|x| Bytes
    uint8_t Unused_1[3];
    uint64_t VolumeSerialNumber;
    uint8_t Unused_2[4];
};

struct NTFS_BOOT_SECTOR
{
    uint8_t Jump[3];
    char OemID[8];
    BIOS_PARAMETER_BLOCK BiosParameterBlock;
    NTFS_EXTENDED_BIOS_PARAMETER_BLOCK ExtendedBiosParameterBlock;
    uint8_t BootstrapCode[426];
    uint8_t EndOfSector[2];     // 55 aa
};

struct NTFS_FILE_RECORD
{
	char Magic[4];				// 'FILE'
	uint16_t UpdateOffset;
	uint16_t UpdateSize;
	uint64_t LogSequenceNumber;
	uint16_t SequenceNumber;
	uint16_t NumberOfLinks;
	uint16_t AttributeOffset;
	struct
	{
		uint16_t InUse : 1;
		uint16_t Directory : 1;
		uint16_t : 14;          // ??
	} Flags;
	uint32_t RealSize;
	uint32_t AllocatedSize;
	uint64_t FileReference;
	uint16_t NextAttribute;
	// --- XP ---
	uint8_t Unused_0[8];
	uint32_t RecordNumber;
	// ...
};

struct NTFS_FILE_ATTRIBUTE
{
	uint32_t Type;
	uint32_t Size;
	uint8_t NonResident;
	uint8_t NameSize;
	uint16_t NameOffset;
	struct
	{
		uint16_t Compressed : 1;
		uint16_t : 13;
		uint16_t Encrypted : 1;
		uint16_t Sparse : 1;
	} Flags;
	uint16_t AttributeID;
};

struct NTFS_FILE_ATTRIBUTE_RESIDENT
{
	// NTFS_FILE_ATTRIBUTE
	uint32_t AttributeSize;
	uint16_t AttributeOffset;
	uint8_t Indexed;
};

struct NTFS_FILE_ATTRIBUTE_NON_RESIDENT
{
	// NTFS_FILE_ATTRIBUTE
	uint64_t StartVCN;
	uint64_t LastVCN;
	uint16_t DataOffset;
	uint16_t CompressionSize;
	uint8_t Unused_0[4];		// 0
	uint64_t AllocatedSize;
	uint64_t RealSize;
	uint64_t DataSize;
};

struct PERMISSIONS
{
	uint32_t ReadOnly : 1;
	uint32_t Hidden : 1;
	uint32_t System : 1;
	uint32_t : 2;
	uint32_t Archive : 1;
	uint32_t Device : 1;
	uint32_t Normal : 1;
	uint32_t Temporary : 1;
	uint32_t Sparse : 1;
	uint32_t ReparsePoint : 1;
	uint32_t Compressed : 1;
	uint32_t Offline : 1;
	uint32_t NotIndexed : 1;
	uint32_t Encrypted : 1;
	uint32_t : 13;
	uint32_t Directory : 1;
	uint32_t IndexView : 1;
	uint32_t : 2;
};

struct NTFS_STANDARD_INFORMATION
{
	uint64_t CreationTime;
	uint64_t AlteredTime;
	uint64_t ModifiedTime;
	uint64_t ReadTime;
	PERMISSIONS Permissions;
	uint32_t MaxVersion;
	uint32_t Version;
	uint32_t ClassID;
	// --- 2k ---
	uint32_t OwnerID;
	uint32_t SecurityID;
	uint64_t QuotaCharged;
	uint64_t UpdateSequenceNumber;
};

struct NTFS_FILE_NAME
{
	uint64_t ParentReference;
	uint64_t CreationTime;
	uint64_t AlteredTime;
	uint64_t ModifiedTime;
	uint64_t ReadTime;
	uint64_t AllocatedSize;
	uint64_t RealSize;
	PERMISSIONS Permissions;
	uint32_t Reparse;
	uint8_t Length;
	uint8_t Namespace;
};

#pragma pack(pop)


void parseMFT(std::unique_ptr<BlockDevice>& device, uint64_t offset)
{
	uint8_t           record[sizeof(NTFS_FILE_RECORD)] = { 0 };
	NTFS_FILE_RECORD* fileRecord = (NTFS_FILE_RECORD*)record;

	device->read(record, offset, sizeof(NTFS_FILE_RECORD));

	debug(L"NTFS_FILE_RECORD {\r\n"
		L"  Magic                    = %c %c %c %c\r\n"
		L"  UpdateOffset             = %hu\r\n"
		L"  UpdateSize               = %hu\r\n"
		L"  LogSequenceNumber        = %llu\r\n"
		L"  SequenceNumber           = %hu\r\n"
		L"  NumberOfLinks            = %hu\r\n"
		L"  AttributeOffset          = %hu\r\n"
		L"  Flags                    = %hu\r\n"
		L"  RealSize                 = %u\r\n"
		L"  AllocatedSize            = %u\r\n"
		L"  FileReference            = %llu\r\n"
		L"  NextAttribute            = %hu\r\n"
		L"  Unused\r\n"
		L"  RecordNumber             = %u\r\n"
		L"}",
		fileRecord->Magic[0], fileRecord->Magic[1], fileRecord->Magic[2], fileRecord->Magic[3],
		fileRecord->UpdateOffset,
		fileRecord->UpdateSize,
		fileRecord->LogSequenceNumber,
		fileRecord->SequenceNumber,
		fileRecord->NumberOfLinks,
		fileRecord->AttributeOffset,
		fileRecord->Flags,
		fileRecord->RealSize,
		fileRecord->AllocatedSize,
		fileRecord->FileReference,
		fileRecord->NextAttribute,
		fileRecord->RecordNumber);

	{
		uint64_t attributeOffset = offset + fileRecord->AttributeOffset;
		uint32_t type = 0;
		uint32_t size = 0;
		do
		{
			device->read(&type, attributeOffset, 4);
			if (type == 0xFFFFFFFF) break;

			device->read(&size, attributeOffset + 4, 4);
			if (size > 0)
			{
				std::unique_ptr<uint8_t, free_delete>   attribute((uint8_t*)malloc(size));
				NTFS_FILE_ATTRIBUTE* fileAttribute = (NTFS_FILE_ATTRIBUTE*)attribute.get();

				device->read(attribute.get(), attributeOffset, size);

				debug(L"NTFS_FILE_ATTRIBUTE {\r\n"
					L"  Type                     = %u\r\n"
					L"  Size                     = %u\r\n"
					L"  NonResident              = %hhu\r\n"
					L"  NameSize                 = %hhu\r\n"
					L"  NameOffset               = %hu\r\n"
					L"  Flags                    = %hu\r\n"
					L"  AttributeID              = %hu\r\n"
					L"}",
					fileAttribute->Type,
					fileAttribute->Size,
					fileAttribute->NonResident,
					fileAttribute->NameSize,
					fileAttribute->NameOffset,
					fileAttribute->Flags,
					fileAttribute->AttributeID);

				if (fileAttribute->NonResident)
				{
					NTFS_FILE_ATTRIBUTE_NON_RESIDENT* nonResidentAttribute = (NTFS_FILE_ATTRIBUTE_NON_RESIDENT*)(attribute.get() + sizeof(NTFS_FILE_ATTRIBUTE));

					debug(L"NTFS_FILE_ATTRIBUTE_NON_RESIDENT {\r\n"
						L"  StartVCN                 = %llu\r\n"
						L"  LastVCN                  = %llu\r\n"
						L"  DataOffset               = %hu\r\n"
						L"  CompressionSize          = %hu\r\n"
						L"  Unused\r\n"
						L"  AllocatedSize            = %llu\r\n"
						L"  RealSize                 = %llu\r\n"
						L"  DataSize                 = %llu\r\n"
						L"}",
						nonResidentAttribute->StartVCN,
						nonResidentAttribute->LastVCN,
						nonResidentAttribute->DataOffset,
						nonResidentAttribute->CompressionSize,
						nonResidentAttribute->AllocatedSize,
						nonResidentAttribute->RealSize,
						nonResidentAttribute->DataSize);

				}
				else
				{
					NTFS_FILE_ATTRIBUTE_RESIDENT* residentAttribute = (NTFS_FILE_ATTRIBUTE_RESIDENT*)(attribute.get() + sizeof(NTFS_FILE_ATTRIBUTE));

					debug(L"NTFS_FILE_ATTRIBUTE_RESIDENT {\r\n"
						L"  AttributeSize            = %u\r\n"
						L"  AttributeOffset          = %hu\r\n"
						L"  Indexed                  = %hhu\r\n"
						L"}",
						residentAttribute->AttributeSize,
						residentAttribute->AttributeOffset,
						residentAttribute->Indexed);

					switch (fileAttribute->Type)
					{
					case 0x10:
						{
							NTFS_STANDARD_INFORMATION* standardInfo = (NTFS_STANDARD_INFORMATION*)(attribute.get() + residentAttribute->AttributeOffset);

							debug(L"NTFS_STANDARD_INFORMATION {\r\n"
								L"  CreationTime             = %llu\r\n"
								L"  AlteredTime              = %llu\r\n"
								L"  ModifiedTime             = %llu\r\n"
								L"  ReadTime                 = %llu\r\n"
								L"  Permissions {\r\n"
								L"    ReadOnly               = %hhu\r\n"
								L"    Hidden                 = %hhu\r\n"
								L"    System                 = %hhu\r\n"
								L"    Archive                = %hhu\r\n"
								L"    Device                 = %hhu\r\n"
								L"    Normal                 = %hhu\r\n"
								L"    Temporary              = %hhu\r\n"
								L"    Sparse                 = %hhu\r\n"
								L"    ReparsePoint           = %hhu\r\n"
								L"    Compressed             = %hhu\r\n"
								L"    Offline                = %hhu\r\n"
								L"    NotIndexed             = %hhu\r\n"
								L"    Encrypted              = %hhu\r\n"
								L"    Directory              = %hhu\r\n"
								L"    IndexView              = %hhu\r\n"
								L"  }\r\n"
								L"  MaxVersion               = %u\r\n"
								L"  Version                  = %u\r\n"
								L"  ClassID                  = %u\r\n"
								L"  OwnerID                  = %u\r\n"
								L"  SecurityID               = %u\r\n"
								L"  QuotaCharged             = %llu\r\n"
								L"  UpdateSequenceNumber     = %llu\r\n"
								L"}",
								standardInfo->CreationTime,
								standardInfo->AlteredTime,
								standardInfo->ModifiedTime,
								standardInfo->ReadTime,
								standardInfo->Permissions.ReadOnly,
								standardInfo->Permissions.Hidden,
								standardInfo->Permissions.System,
								standardInfo->Permissions.Archive,
								standardInfo->Permissions.Device,
								standardInfo->Permissions.Normal,
								standardInfo->Permissions.Temporary,
								standardInfo->Permissions.Sparse,
								standardInfo->Permissions.ReparsePoint,
								standardInfo->Permissions.Compressed,
								standardInfo->Permissions.Offline,
								standardInfo->Permissions.NotIndexed,
								standardInfo->Permissions.Encrypted,
								standardInfo->Permissions.Directory,
								standardInfo->Permissions.IndexView,
								standardInfo->MaxVersion,
								standardInfo->Version,
								standardInfo->ClassID,
								standardInfo->OwnerID,
								standardInfo->SecurityID,
								standardInfo->QuotaCharged,
								standardInfo->UpdateSequenceNumber);
						}
						break;

					case 0x30:
					{
						NTFS_FILE_NAME* fileName = (NTFS_FILE_NAME*)(attribute.get() + residentAttribute->AttributeOffset);

						debug(L"NTFS_FILE_NAME {\r\n"
							L"  ParentReference          = %llu\r\n"
							L"  CreationTime             = %llu\r\n"
							L"  AlteredTime              = %llu\r\n"
							L"  ModifiedTime             = %llu\r\n"
							L"  ReadTime                 = %llu\r\n"
							L"  AllocatedSize            = %llu\r\n"
							L"  RealSize                 = %llu\r\n"
							L"  Permissions {\r\n"
							L"    ReadOnly               = %hhu\r\n"
							L"    Hidden                 = %hhu\r\n"
							L"    System                 = %hhu\r\n"
							L"    Archive                = %hhu\r\n"
							L"    Device                 = %hhu\r\n"
							L"    Normal                 = %hhu\r\n"
							L"    Temporary              = %hhu\r\n"
							L"    Sparse                 = %hhu\r\n"
							L"    ReparsePoint           = %hhu\r\n"
							L"    Compressed             = %hhu\r\n"
							L"    Offline                = %hhu\r\n"
							L"    NotIndexed             = %hhu\r\n"
							L"    Encrypted              = %hhu\r\n"
							L"    Directory              = %hhu\r\n"
							L"    IndexView              = %hhu\r\n"
							L"  }\r\n"
							L"  Reparse                  = %u\r\n"
							L"  Length                   = %hhu\r\n"
							L"  Namespace                = %hhu\r\n"
							L"}",
							fileName->ParentReference,
							fileName->CreationTime,
							fileName->AlteredTime,
							fileName->ModifiedTime,
							fileName->ReadTime,
							fileName->AllocatedSize,
							fileName->RealSize,
							fileName->Permissions.ReadOnly,
							fileName->Permissions.Hidden,
							fileName->Permissions.System,
							fileName->Permissions.Archive,
							fileName->Permissions.Device,
							fileName->Permissions.Normal,
							fileName->Permissions.Temporary,
							fileName->Permissions.Sparse,
							fileName->Permissions.ReparsePoint,
							fileName->Permissions.Compressed,
							fileName->Permissions.Offline,
							fileName->Permissions.NotIndexed,
							fileName->Permissions.Encrypted,
							fileName->Permissions.Directory,
							fileName->Permissions.IndexView,
							fileName->Reparse,
							fileName->Length,
							fileName->Namespace);
						{
							std::unique_ptr<uint8_t, free_delete>   name((uint8_t*)malloc((fileName->Length + 1) * 2));
							memset(name.get(), 0, (fileName->Length + 1) * 2);
							memcpy(name.get(), attribute.get() + residentAttribute->AttributeOffset + sizeof(NTFS_FILE_NAME), fileName->Length * 2);
							debug(L"File Name: %ls", (wchar_t*)name.get());
						}
					}
					break;
					}
				}

				attributeOffset += size;
			} else break;
		} while (attributeOffset - offset < fileRecord->RealSize);
	}
}


void parseNTFS(std::unique_ptr<BlockDevice> &device, uint8_t header[512])
{
    NTFS_BOOT_SECTOR*   bootSector = (NTFS_BOOT_SECTOR*)header;
    char*               oemID[9] = { 0 };
    uint32_t            clusterSize = 0;
    uint64_t            mftOffset = 0;
    uint64_t            backupOffset = 0;
    uint32_t            mftRecordSize = 0;
    uint32_t            indexBufferSize = 0;

    // zero terminate
    memcpy(oemID, bootSector->OemID, 8);

    debug(L"NTFS_BOOT_SECTOR {\r\n"
        L"  Jump                     = %02hx %02hx %02hx\r\n"
        L"  OemID                    = %hs\r\n"
        L"  BiosParameterBlock {\r\n"
        L"    BytesPerSector         = %hu\r\n"
        L"    SectorsPerCluster      = %hu\r\n"
        L"    ReservedSectors        = %hu\r\n"
        L"    NumberOfFATs           = %hu\r\n"
        L"    RootDirectories        = %hu\r\n"
        L"    TotalSectors           = %hu\r\n"
        L"    MediaDescriptor        = %02hx\r\n"
        L"    SectorsPerFAT          = %hu\r\n"
        L"    SectorsPerTrack        = %hu\r\n"
        L"    NumberOfHeads          = %hu\r\n"
        L"    HiddenSectors32        = %u\r\n"
        L"    TotalSectors32         = %u\r\n"
        L"  }\r\n"
        L"  ExtendedBiosParameterBlock {\r\n"
        L"    SectorsPerFAT32        = %u\r\n"
        L"    TotalSectors64         = %llu\r\n"
        L"    MFTLocation            = %llu\r\n"
        L"    BackupMFTLocation      = %llu\r\n"
        L"    ClustersPerMFTRecord   = %hd\r\n"
        L"    Unused\r\n"
        L"    ClustersPerIndexBuffer = %hd\r\n"
        L"    Unused\r\n"
        L"    VolumeSerialNumber     = %016llx\r\n"
        L"    Unused\r\n"
        L"  }\r\n"
        L"  BootstrapCode\r\n"
        L"  EndOfSector              = %02hx %02hx\r\n"
        L"}",
        bootSector->Jump[0], bootSector->Jump[1], bootSector->Jump[2],
        oemID,
        bootSector->BiosParameterBlock.BytesPerSector,
        bootSector->BiosParameterBlock.SectorsPerCluster,
        bootSector->BiosParameterBlock.ReservedSectors,
        bootSector->BiosParameterBlock.NumberOfFATs,
        bootSector->BiosParameterBlock.RootDirectories,
        bootSector->BiosParameterBlock.TotalSectors,
        bootSector->BiosParameterBlock.MediaDescriptor,
        bootSector->BiosParameterBlock.SectorsPerFAT,
        bootSector->BiosParameterBlock.SectorsPerTrack,
        bootSector->BiosParameterBlock.NumberOfHeads,
        bootSector->BiosParameterBlock.HiddenSectors32,
        bootSector->BiosParameterBlock.TotalSectors32,
        bootSector->ExtendedBiosParameterBlock.SectorsPerFAT32,
        bootSector->ExtendedBiosParameterBlock.TotalSectors64,
        bootSector->ExtendedBiosParameterBlock.MFTLocation,
        bootSector->ExtendedBiosParameterBlock.BackupMFTLocation,
        bootSector->ExtendedBiosParameterBlock.ClustersPerMFTRecord,
        bootSector->ExtendedBiosParameterBlock.ClustersPerIndexBuffer,
        bootSector->ExtendedBiosParameterBlock.VolumeSerialNumber,
        bootSector->EndOfSector[0], bootSector->EndOfSector[1]);

    clusterSize = bootSector->BiosParameterBlock.BytesPerSector * bootSector->BiosParameterBlock.SectorsPerCluster;
    mftOffset = bootSector->ExtendedBiosParameterBlock.MFTLocation * clusterSize;
    backupOffset = bootSector->ExtendedBiosParameterBlock.BackupMFTLocation * clusterSize;
    if (bootSector->ExtendedBiosParameterBlock.ClustersPerMFTRecord > 0)
    {
        mftRecordSize = bootSector->ExtendedBiosParameterBlock.ClustersPerMFTRecord * clusterSize;
    }
    else
    {
        mftRecordSize = (uint32_t)pow((double)2, abs(bootSector->ExtendedBiosParameterBlock.ClustersPerMFTRecord));
    }
    if (bootSector->ExtendedBiosParameterBlock.ClustersPerIndexBuffer > 0)
    {
        indexBufferSize = bootSector->ExtendedBiosParameterBlock.ClustersPerIndexBuffer * clusterSize;
    }
    else
    {
        indexBufferSize = (uint32_t)pow((double)2, abs(bootSector->ExtendedBiosParameterBlock.ClustersPerIndexBuffer));
    }

    debug(L"Cluster Size: %u\r\n"
        L"Primary MFT - Offset: %llu\r\n"
        L"Backup  MFT - Offset: %llu\r\n"
        L"MFT Record Size: %u\r\n"
        L"Index Buffer Size: %u",
        clusterSize, mftOffset, backupOffset, mftRecordSize, indexBufferSize);

	//parseMFT(device, mftOffset);
	parseMFT(device, backupOffset);
}
