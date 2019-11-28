use std::io;
use std::io::{Read, Seek};

use super::fat::BIOS_PARAMETER_BLOCK;
use super::super::device::{Block, BlockDevice};

#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
struct NTFS_EXTENDED_BIOS_PARAMETER_BLOCK {
    // BIOS_PARAMETER_BLOCK
    SectorsPerFAT32: u32,       // 0 for NTFS
    TotalSectors64: u64,
    MFTLocation: u64,           // Logical Cluster Number
    BackupMFTLocation: u64,     // Logical Cluster Number
    ClustersPerMFTRecord: i8,   // Positive: Number of Clusters, Negative: 2^|x| Bytes
    Unused_0: [u8; 3],
    ClustersPerIndexBuffer: i8, // Positive: Number of Clusters, Negative: 2^|x| Bytes
    Unused_1: [u8; 3],
    VolumeSerialNumber: u64,
    Unused_2: [u8; 4],
}

#[allow(non_snake_case)]
#[derive(Clone, Copy)]
#[repr(C, packed)]
struct NTFS_BOOT_SECTOR {
    Jump: [u8; 3],
    OemID: [u8; 8],             // 'NTFS    '
    BiosParameterBlock: BIOS_PARAMETER_BLOCK,
    ExtendedBiosParameterBlock: NTFS_EXTENDED_BIOS_PARAMETER_BLOCK,
    BootstrapCode: [u8; 426],
    EndOfSector: [u8; 2],       // 55 aa
}

#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
struct MULTI_SECTOR_HEADER {
    Signature: [u8; 4],         // 'FILE'
    UpdateSequenceArrayOffset: u16,
    UpdateSequenceArraySize: u16,
}

#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
struct FILE_REFERENCE {
    SegmentNumberLowPart: u32,
    SegmentNumberHighPart: u16,
    SequenceNumber: u16,
}

const FILE_RECORD_SEGMENT_IN_USE: u16 = 0x0001;
const FILE_FILE_NAME_INDEX_PRESENT: u16 = 0x0002;
const FILE_RECORD_IN_EXTEND: u16 = 0x0004;
const FILE_RECORD_IS_VIEW_INDEX: u16 = 0x0008;

#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
struct FILE_RECORD_SEGMENT_HEADER {
    MultiSectorHeader: MULTI_SECTOR_HEADER,
    LogSequenceNumber: u64,     // Reserved1
    SequenceNumber: u16,        // 0 => record is unused
    ReferenceCount: u16,        // Reserved2
    FirstAttributeOffset: u16,
    Flags: u16,                 // FILE_*
    RealSize: u32,              // Reserved3[0]
    AllocatedSize: u32,         // Reserved3[1]
    BaseFileRecordSegment: FILE_REFERENCE,
    NextAttribute: u16,
}

#[allow(non_camel_case_types)]
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(u32)]
enum ATTRIBUTE_TYPE_CODE {
    STANDARD_INFORMATION = 0x10,
    ATTRIBUTE_LIST = 0x20,
    FILE_NAME = 0x30,
    OBJECT_ID = 0x40,
    VOLUME_NAME = 0x60,
    VOLUME_INFORMATION = 0x70,
    DATA = 0x80,
    INDEX_ROOT = 0x90,
    INDEX_ALLOCATION = 0xA0,
    BITMAP = 0xB0,
    REPARSE_POINT = 0xC0,
    END = 0xFFFFFFFF,
}

const RESIDENT_FORM: u8 = 0x00;
const NONRESIDENT_FORM: u8 = 0x01;

const ATTRIBUTE_FLAG_COMPRESSION_MASK: u16 = 0x00FF;
const ATTRIBUTE_FLAG_SPARSE: u16 = 0x8000;
const ATTRIBUTE_FLAG_ENCRYPTED: u16 = 0x4000;

#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
struct ATTRIBUTE_RECORD_HEADER {
    TypeCode: ATTRIBUTE_TYPE_CODE,
    RecordLength: u32,
    FormCode: u8,               // *_FORM
    NameLength: u8,
    NameOffset: u16,
    Flags: u16,
    Instance: u16,
    // ...
}


fn parse_file_attribute<R>(device: &mut BlockDevice<R>, offset: u64, max_size: u64) -> io::Result<u64> where R: Block + Read + Seek {
    read_struct!(ATTRIBUTE_TYPE_CODE, device, offset, max_size).and_then(|type_code| {
        if type_code == ATTRIBUTE_TYPE_CODE::END {
            Ok(0)
        } else {
            read_struct!(ATTRIBUTE_RECORD_HEADER, device, offset, max_size).and_then(|attr| {
                Ok(attr.RecordLength as u64)
            })
        }
    }).or_else(|err| Err(err))
}

fn parse_file_record<R>(device: &mut BlockDevice<R>, offset: u64, max_size: u64) -> io::Result<()> where R: Block + Read + Seek {
    read_struct!(FILE_RECORD_SEGMENT_HEADER, device, offset, max_size).and_then(|record| {

        let (max, mut pos) = (record.RealSize as u64, record.FirstAttributeOffset as u64);
        while pos < max {
            let result = parse_file_attribute(device, offset + pos, max - pos);
            if let Err(err) = result {
                return Err(err);
            }
            let size = result.unwrap();
            if size == 0 {
                break;
            }
            pos += size;
        }

        Ok(())
    }).or_else(|err| Err(err))
}

pub fn parse_ntfs<R>(device: BlockDevice<R>, header: &[u8]) -> io::Result<()> where R: Block + Read + Seek {
    let boot_sector: &NTFS_BOOT_SECTOR = unsafe{ & *(header.as_ptr() as *const NTFS_BOOT_SECTOR) };

    debug!("{:#?}", boot_sector.BiosParameterBlock);
    debug!("{:#?}", boot_sector.ExtendedBiosParameterBlock);

    let cluster_size = boot_sector.BiosParameterBlock.BytesPerSector as u64 *
                       boot_sector.BiosParameterBlock.SectorsPerCluster as u64;
    let mft_offset = boot_sector.ExtendedBiosParameterBlock.MFTLocation * cluster_size;
    let backup_offset = boot_sector.ExtendedBiosParameterBlock.BackupMFTLocation * cluster_size;
    let mft_record_size = if boot_sector.ExtendedBiosParameterBlock.ClustersPerMFTRecord > 0 {
        boot_sector.ExtendedBiosParameterBlock.ClustersPerMFTRecord as u64 * cluster_size
    } else {
        (2 as u64).pow((-boot_sector.ExtendedBiosParameterBlock.ClustersPerMFTRecord) as u32)
    };
    let index_buffer_size = if boot_sector.ExtendedBiosParameterBlock.ClustersPerIndexBuffer > 0 {
        boot_sector.ExtendedBiosParameterBlock.ClustersPerIndexBuffer as u64 * cluster_size
    } else {
        (2 as u64).pow((-boot_sector.ExtendedBiosParameterBlock.ClustersPerIndexBuffer) as u32)
    };

    debug!(
        "Cluster Size: {}\nPrimary MFT - Offset: {}\nBackup  MFT - Offset: {}\nMFT Record Size: {}\nIndex Buffer Size: {}",
        cluster_size, mft_offset, backup_offset, mft_record_size, index_buffer_size
    );

    let mut wrapped = BlockDevice::with_block_size(device, cluster_size as usize);

    parse_file_record(&mut wrapped, mft_offset, mft_record_size).or_else(|_| {
        eprintln!("WARNING: Primart MFT is bad. Parsing backup...");
        parse_file_record(&mut wrapped, backup_offset, mft_record_size)
    })
}