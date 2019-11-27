use std::io::Read;

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


pub fn parse_ntfs<R>(mut device: BlockDevice<R>, header: &[u8]) where R: Block + Read {
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
}