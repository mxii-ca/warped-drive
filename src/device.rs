use std::fs;
use std::io;
use std::mem;
use std::ptr;


/// A Block Reader.
#[derive(Debug)]
pub struct BlockDevice<R> {
    inner: R,
    sector_size: u32
}

impl<R> BlockDevice<R> {

    /// Creates a new `BlockDevice`.
    pub fn new(inner: R) -> BlockDevice<R> {
        BlockDevice{ inner: inner, sector_size: 0 }
    }

    /// Gets a reference to the underlying reader.
    pub fn get_ref(&self) -> &R { &self.inner }

    /// Gets a mutable reference to the underlying reader.
    pub fn get_mut(&mut self) -> &mut R { &mut self.inner }

    /// Unwraps this `BlockDevice`, returning the underlying reader.
    pub fn into_inner(self) -> R { self.inner }
}

impl BlockDevice<fs::File> {

    /// Open a block device or file
    pub fn open(path: &str) -> io::Result<BlockDevice<fs::File>> {
        eprintln!("Opening: {}", path);
        fs::File::open(path).and_then(|file| { Ok(BlockDevice::new(file)) })
    }

    #[cfg(windows)]
    fn _get_sector_size(&self) -> io::Result<u32> {
        use std::os::windows::io::AsRawHandle;
        use winapi::shared::minwindef::{DWORD, ULONG};
        use winapi::shared::ntdef::PVOID;
        use winapi::shared::winerror;
        use winapi::um::errhandlingapi::GetLastError;
        use winapi::um::ioapiset::DeviceIoControl;
        use winapi::um::winioctl;
        use winapi::um::winnt::HANDLE;

        winapi::STRUCT! {
            #[derive(Debug)]
            #[allow(non_snake_case)]
            struct STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR {
                Version: ULONG,
                Size: ULONG,
                BytesPerCacheLine: ULONG,
                BytesOffsetForCacheAlignment: ULONG,
                BytesPerLogicalSector: ULONG,
                BytesPerPhysicalSector: ULONG,
                BytesOffsetForSectorAlignment: ULONG,
            }
        }

        let mut query = winioctl::STORAGE_PROPERTY_QUERY {
            PropertyId: winioctl::StorageAccessAlignmentProperty,
            QueryType: winioctl::PropertyStandardQuery,
            AdditionalParameters: [0]
        };
        let mut alignment: STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR = unsafe { mem::zeroed() };
        let mut size = 0;
        let ret = unsafe {
            DeviceIoControl(
                self.inner.as_raw_handle() as HANDLE,
                winioctl::IOCTL_STORAGE_QUERY_PROPERTY,
                &mut query as *mut _ as PVOID,
                mem::size_of_val(&query) as DWORD,
                &mut alignment as *mut _ as PVOID,
                mem::size_of_val(&alignment) as DWORD,
                &mut size,
                ptr::null_mut()
            )
        };
        if ! winerror::SUCCEEDED(ret) {
            let err = std::io::Error::from_raw_os_error(unsafe{ GetLastError() } as i32);
            eprintln!("IOCTL_STORAGE_QUERY_PROPERTY Failed: {}", err);

            // IOCTL_STORAGE_QUERY_PROPERTY doesn't work for external drives
            // fallback to the logical sector size

            winapi::STRUCT! {
                #[derive(Debug)]
                #[allow(non_snake_case)]
                struct DISK_GEOMETRY {
                    Cylinders: u64,  // LARGE_INTEGER
                    MediaType: winioctl::MEDIA_TYPE,
                    TracksPerCylinder: DWORD,
                    SectorsPerTrack: DWORD,
                    BytesPerSector: DWORD,
                }
            }

            let mut geometry : DISK_GEOMETRY = unsafe { mem::zeroed() };
            let ret = unsafe {
                DeviceIoControl(
                    self.inner.as_raw_handle() as HANDLE,
                    winioctl::IOCTL_DISK_GET_DRIVE_GEOMETRY,
                    ptr::null_mut(),
                    0,
                    &mut geometry as *mut _ as PVOID,
                    mem::size_of_val(&geometry) as DWORD,
                    &mut size,
                    ptr::null_mut()
                )
            };
            if ! winerror::SUCCEEDED(ret) {
                let err = std::io::Error::from_raw_os_error(unsafe{ GetLastError() } as i32);
                eprintln!("IOCTL_DISK_GET_DRIVE_GEOMETRY Failed: {}", err);
                Err(err)
            } else {
                eprintln!("{:?}", &geometry);
                Ok(geometry.BytesPerSector)
            }
        } else {
            eprintln!("{:?}", &alignment);
            Ok(alignment.BytesPerPhysicalSector)
        }
    }

    #[cfg(unix)]
    fn _get_sector_size(&self) -> io::Result<u32> {
        use nix::sys::ioctl;
        use std::os::unix::io::IntoRawFd;

        const BLKBSZGET_TYPE: u8 = 0x12;
        const BLKBSZGET_NUMBER: u8 = 112;
        ioctl::ioctl!(read get_block_size with BLKBSZGET_TYPE, BLKBSZGET_NUMBER; usize);

        let fd = self.inner.into_raw_fd();

        get_block_size(fd, &mut sector_size).or_else(|err| {
            eprintln!("IOCTL BLKBSZGET Failed: {}", err);
            err
        })
    }

    /// Get the sector size of the device.
    pub fn get_sector_size(&mut self) -> u32 {
        if self.sector_size == 0 {
            self.sector_size = self._get_sector_size().unwrap_or(512);
        }
        self.sector_size
    }
}

impl<R: io::Read> io::Read for BlockDevice<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        Ok(0)
    }
}