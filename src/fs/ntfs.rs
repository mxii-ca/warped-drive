use std::io::Read;
use std::mem;

use super::fat::BIOS_PARAMETER_BLOCK;
use super::super::device::{Block, BlockDevice};

#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
#[allow(non_snake_case)]
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


pub fn parse_ntfs<R>(mut device: BlockDevice<R>, header: &[u8]) where R: Block + Read {
    let boot_sector: &NTFS_BOOT_SECTOR = unsafe{ mem::transmute(header.as_ptr()) };

    eprintln!("{:#?}\n", boot_sector.BiosParameterBlock);
    eprintln!("{:#?}\n", boot_sector.ExtendedBiosParameterBlock);
}