use core::cmp;
use std::io;
use std::io::{Read, Seek};

use super::fat::BIOS_PARAMETER_BLOCK;
use super::super::device::{Block, Device, Volume};

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

}

}


pub struct Ntfs<R> {
    inner: Device<R>,
    mft: FileRecord,
    record_size: u64,
}

impl<R> Ntfs<R> {

    /// Gets a reference to the underlying reader.
    pub fn get_ref(&self) -> &R { self.inner.get_ref() }

    /// Gets a mutable reference to the underlying reader.
    pub fn get_mut(&mut self) -> &mut R { self.inner.get_mut() }

    /// Unwraps this `BlockDevice`, returning the underlying reader.
    pub fn into_inner(self) -> R { self.inner.into_inner() }
}

impl<R> Volume<R> for Ntfs<R>
where R: Read + Seek {

    fn is_supported(header: &[u8; 512]) -> bool {
        &header[3..7] == b"NTFS"
    }

    fn with_header(inner: R, header: &[u8; 512]) -> io::Result<Self> {
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

        let mut wrapper = Device::with_block_size(inner, cluster_size as usize);

        let mft_record = {
            FileRecord::new(&mut wrapper, mft_offset, mft_record_size).or_else(|_| {
                eprintln!("WARNING: Primart MFT is bad. Parsing backup...");
                FileRecord::new(&mut wrapper, backup_offset, mft_record_size)
            })
        }?;

        Ok(Self {
            inner: wrapper,
            record_size: mft_record_size,
            mft: mft_record,
        })
    }

    fn refresh(&mut self) -> io::Result<()> {
        self.mft.refresh(&mut self.inner, self.record_size)
    }
}


struct FileRecord {
    offset: u64,
}

impl FileRecord {

    pub fn new<R>(device: &mut Device<R>, offset: u64, size: u64) -> io::Result<Self>
    where R: Read + Seek {
        let mut new = Self{ offset: offset };
        new.refresh(device, size)?;
        Ok(new)
    }

    pub fn refresh<R>(&mut self, device: &mut Device<R>, size: u64) -> io::Result<()>
    where R: Read + Seek {
        let record = read_struct!(FILE_RECORD_SEGMENT_HEADER, device, self.offset, size)?;

        let max = cmp::min(size, record.RealSize as u64);
        let mut pos = record.FirstAttributeOffset as u64;
        while pos < max {
            let size = self.parse_attribute(device, self.offset + pos, max - pos)?;
            if size == 0 {
                break;
            }
            pos += size;
        }

        Ok(())
    }

    fn parse_attribute<R>(&mut self, device: &mut Device<R>, offset: u64, max_size: u64) -> io::Result<u64>
    where R: Read + Seek {
        let type_code = read_struct!(ATTRIBUTE_TYPE_CODE, device, offset, max_size)?;
        if type_code == ATTRIBUTE_TYPE_CODE::END {
            return Ok(0);
        }

        let attr = read_struct!(ATTRIBUTE_RECORD_HEADER, device, offset, max_size)?;

        #[cfg(debug_assertions)]
        let name = read_utf16!(device, offset + attr.NameOffset as u64, attr.NameLength, max_size - attr.NameOffset as u64)?;
        debug!("Attribute Name: {}", name);

        Ok(attr.RecordLength as u64)
    }
}