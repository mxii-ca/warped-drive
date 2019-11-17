use std::cmp;
use std::fs;
use std::io;
use std::io::{BufRead, Read, Seek, SeekFrom};
use std::mem;
use std::ptr;


fn iadd(lvalue: u64, rvalue: i64) -> io::Result<u64> {
    let result = if rvalue > 0 {
        lvalue.checked_add(rvalue as u64)
    } else {
        lvalue.checked_sub((-rvalue) as u64)
    };
    if let Some(value) = result {
        Ok(value)
    } else {
        Err(io::Error::from(io::ErrorKind::InvalidInput))
    }
}


/// A trait for objects where all operations are in multiples of a fixed size.
pub trait Block {

    /// Get the block size in bytes.
    fn get_block_size(&self) -> io::Result<usize>;
}

impl Block for fs::File {

    #[cfg(windows)]
    fn get_block_size(&self) -> io::Result<usize> {
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
                self.as_raw_handle() as HANDLE,
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
                    self.as_raw_handle() as HANDLE,
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
                Ok(geometry.BytesPerSector as usize)
            }
        } else {
            eprintln!("{:?}", &alignment);
            Ok(alignment.BytesPerPhysicalSector as usize)
        }
    }

    #[cfg(unix)]
    fn get_block_size(&self) -> io::Result<usize> {
        use nix::sys::ioctl;
        use std::os::unix::io::IntoRawFd;

        const BLKBSZGET_TYPE: u8 = 0x12;
        const BLKBSZGET_NUMBER: u8 = 112;
        ioctl::ioctl!(read get_block_size with BLKBSZGET_TYPE, BLKBSZGET_NUMBER; usize);

        let fd = self.into_raw_fd();

        let result = get_block_size(fd, &mut sector_size);
        if let Err(err) = result {
            eprintln!("IOCTL BLKBSZGET Failed: {}", err);
        }
        result
    }
}


/// A Block Reader.
#[derive(Debug)]
pub struct BlockDevice<R> {
    inner: io::BufReader<R>,
    block_size: usize,
}

impl<R> BlockDevice<R> where R: Read {

    /// Creates a new `BlockDevice` with the specified block size.
    pub fn with_block_size(inner: R, block_size: usize) -> BlockDevice<R> {
        BlockDevice{
            inner: io::BufReader::with_capacity(block_size, inner),
            block_size: block_size,
        }
    }

    /// Gets a reference to the underlying reader.
    pub fn get_ref(&self) -> &R { &self.inner.get_ref() }

    /// Gets a mutable reference to the underlying reader.
    pub fn get_mut(&mut self) -> &mut R { self.inner.get_mut() }

    /// Unwraps this `BlockDevice`, returning the underlying reader.
    pub fn into_inner(self) -> R { self.inner.into_inner() }
}

impl<R> BlockDevice<R> where R: Block + Read {

    /// Creates a new `BlockDevice`.
    pub fn new(inner: R) -> io::Result<BlockDevice<R>> {

        let result = inner.get_block_size();
        if let Err(err) = result { return Err(err); }
        let block_size = result.unwrap();

        Ok(BlockDevice::with_block_size(inner, block_size))
    }
}

impl BlockDevice<fs::File> {

    /// Open a block device or file
    pub fn open(path: &str) -> io::Result<BlockDevice<fs::File>> {
        eprintln!("Opening: {}", path);

        let result = fs::File::open(path);
        if let Err(err) = result { return Err(err); }
        let file = result.unwrap();

        BlockDevice::new(file)
    }
}

impl<R> Block for BlockDevice<R> where R: Block {
    fn get_block_size(&self) -> io::Result<usize> {
        Ok(self.block_size)
    }
}

impl<R> Read for BlockDevice<R> where R: Read {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let size = buf.len();
        let cached = self.inner.buffer().len();

        // consume the cache first
        if cached > 0 {
            self.inner.read(&mut buf[..cmp::min(size, cached)])

        // `self.inner` (`BufReader`) will always read exactly `self.block_size`
        // bytes except when asked to read a larger size in which case the
        // size must be truncated to ensure an aligned read
        } else if size > self.block_size {
            let aligned = size - (size % self.block_size);
            self.inner.read(&mut buf[..aligned])

        // otherwise, can let `self.inner` handle the read
        } else {
            self.inner.read(buf)
        }
    }
}

impl<R> Seek for BlockDevice<R> where R: Read + Seek {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {

        // `self.inner` (`BufReader`) will always clear the buffer on seek.
        // get the current position by looking at the inner inner stream
        // to avoid clearing the cache and the remaining size of the cache buffer
        let result = self.inner.get_mut().seek(SeekFrom::Current(0));
        if let Err(err) = result { return Err(err); }
        let maximum = result.unwrap();
        let remain = self.inner.buffer().len();
        let current = maximum - remain as u64;

        // figure out the target location
        let result = match pos {
            SeekFrom::Current(n) => { iadd(current, n) }
            SeekFrom::Start(n) => { Ok(n) }
            SeekFrom::End(n) => {
                let result = self.inner.seek(SeekFrom::End(0));
                if let Err(err) = result { return Err(err); }
                let end = result.unwrap();

                iadd(end, n)
            }
        };
        if let Err(err) = result { return Err(err); }
        let target = result.unwrap();

        // use `consume` to *seek* within the confines of the cache
        // but only forwards, (it takes a `usize`)
        if target >= current && current <= maximum {
            self.inner.consume((target - current) as usize);
        } else {

            // `self.inner` (`BufReader`) will seek to any value.
            // so only seek to aligned values to ensure all reads are aligned
            let offset = target % self.block_size as u64;
            let aligned = target - offset;

            let result = self.inner.seek(SeekFrom::Start(aligned));
            if let Err(err) = result { return Err(err); }

            // since `seek` cleared the cache, repopulate it
            // so that `consume` can be used to *seek* to the exact target
            if offset > 0 {
                let result = self.inner.fill_buf();
                if let Err(err) = result { return Err(err); }

                self.inner.consume(offset as usize);
            }
        }
        Ok(target)
    }
}