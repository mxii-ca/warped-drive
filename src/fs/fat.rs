
// unfortunately, Rust does not yet have features condusive to C struct interoperability
// - RFC: Bit Fields
//   https://github.com/rust-lang/rfcs/issues/314
// - RFC: Structural Records    ie. anonymous structs
//   https://github.com/rust-lang/rfcs/pull/2584

// also unfortunately, Rust can't derive 'Debug' for `union`
// or for arrays larger than 32


#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
pub struct BIOS_PARAMETER_BLOCK {
    // --- DOS 2.0 ---
    BytesPerSector: u16,
    SectorsPerCluster: u8,
    ReservedSectors: u16,   // 0 for NTFS
    NumberOfFATs: u8,       // 0 for NTFS
    RootDirectories: u16,   // 0 for NTFS, FAT32
    TotalSectors: u16,      // 0 for NTFS, FAT32
    MediaDescriptor: u8,
    SectorsPerFAT: u16,     // 0 for NTFS, FAT32
    // --- DOS 3.0 ---
    SectorsPerTrack: u16,
    NumberOfHeads: u16,
    // union {
    //     struct: u32 {
    //         HiddenSectors: u16,
    // --- DOS 3.2 ---
    //         TotalAndHiddenSectors: u16,
    //     },
    // --- DOS 3.31 ---
    HiddenSectors32: u32,
    // },
    TotalSectors32: u32,    // 0 for NTFS
}

const CHKDSK_FLAG_VOLUME_DIRTY: u8 = 0x01;
const CHKDSK_FLAG_SURFACE_SCAN: u8 = 0x02;

#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
#[allow(non_snake_case)]
struct EXTENDED_BIOS_PARAMETER_BLOCK {
    // BIOS_PARAMETER_BLOCK
    // --- DOS 4.0 ---
    DriveNumber: u8,
    ChkdskFlags: u8,        // CHKDSK_FLAG_*
    ExtendedParameters: u8,
    // ExtendedParameters == 0x28 or 0x29
    VolumeID: [u8; 4],      // xxxx-xxxx
    // ExtendedParameters == 0x29
    VolumeLabel: [u8; 11],
    FilesystemType: [u8; 8],
}

const MIRROR_MASK_ACTIVE_FATS: u16 = 0x000F;
const MIRROR_FLAG_ALL_FATS: u16 = 0x0080;

#[derive(Clone, Copy, Debug)]
#[repr(C, packed)]
#[allow(non_snake_case)]
struct FAT32_EXTENDED_BIOS_PARAMETER_BLOCK {
    // BIOS_PARAMETER_BLOCK
    // --- DOS 7.1 ---
    SectorsPerFAT32: u32,
    MirrorFlags: u16,           // MIRROR_[MASK|FLAG]_*
    Version: [u8; 2],           // 0.0
    RootLocation: u32,          // Physical Cluster Number
    FSInformationLocation: u16, // Logical Sector Number
    BackupLocation: u16,        // Logical Sector Number, 0xFFFF if None
    Reserved: [u8; 12],
    // EXTENDED_BIOS_PARAMETER_BLOCK
}