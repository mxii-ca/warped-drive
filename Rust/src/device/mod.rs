use core::cmp;
use std::fs;
use std::io;
use std::io::{BufRead, Read, Seek, SeekFrom};

#[cfg_attr(unix, path = "unix.rs")]
#[cfg_attr(windows, path = "windows.rs")]
mod os;

use super::utils::iadd;


/// A trait for objects where all operations are in multiples of a fixed size.
pub trait Block {

    /// Get the block size in bytes.
    fn get_block_size(&self) -> io::Result<usize>;
}


/// A Block Reader.
#[derive(Debug)]
pub struct Device<R> {
    // ideally we would wrap this in a `BufReader` so that it can handle blocking
    // unfortunately, `BufReader.seek` always drops the buffer
    inner: R,
    buf: Box<[u8]>,
    pos: usize,
    cap: usize,
}

impl<R> Device<R> {

    /// Creates a new `BlockDevice` with the specified block size.
    pub fn with_block_size(inner: R, block_size: usize) -> Self {
        let mut buffer = Vec::with_capacity(block_size);
        buffer.resize(block_size, 0);
        Self {
            inner: inner,
            buf: buffer.into_boxed_slice(),
            pos: 0,
            cap: 0,
        }
    }

    /// Gets a reference to the underlying reader.
    pub fn get_ref(&self) -> &R { &self.inner }

    /// Gets a mutable reference to the underlying reader.
    pub fn get_mut(&mut self) -> &mut R { &mut self.inner }

    /// Unwraps this `BlockDevice`, returning the underlying reader.
    pub fn into_inner(self) -> R { self.inner }

    /// Returns a reference to the internally buffered data.
    pub fn buffer(&self) -> &[u8] {
        &self.buf[self.pos..self.cap]
    }

    /// Invalidates all data in the internal buffer.
    #[inline]
    fn discard_buffer(&mut self) {
        self.pos = 0;
        self.cap = 0;
    }
}

impl<R> Device<R>
where R: Block {

    /// Creates a new `BlockDevice`.
    pub fn new(inner: R) -> io::Result<Self> {
        let block_size = inner.get_block_size()?;
        Ok(Self::with_block_size(inner, block_size))
    }
}

impl Device<fs::File> {

    /// Open a block device or file
    pub fn open(path: &str) -> io::Result<Self> {
        debug!("Opening: {}", path);
        let file = fs::File::open(path)?;
        Self::new(file)
    }
}

impl<R> Block for Device<R> {
    fn get_block_size(&self) -> io::Result<usize> {
        Ok(self.buf.len())
    }
}

impl<R> BufRead for Device<R>
where R: Read {
    fn fill_buf(&mut self) -> io::Result<&[u8]> {
        if self.pos >= self.cap {
            debug_assert!(self.pos == self.cap);
            self.cap = self.inner.read(&mut self.buf)?;
            self.pos = 0;
        }
        Ok(&self.buf[self.pos..self.cap])
    }

    fn consume(&mut self, amt: usize) {
        self.pos = cmp::min(self.pos + amt, self.cap);
    }
}

impl<R> Read for Device<R>
where R: Read {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {

        // bypass the cache for large reads
        if self.pos == self.cap && buf.len() > self.buf.len() {
            let aligned = buf.len() - (buf.len() % self.buf.len());
            self.discard_buffer();
            return self.inner.read(&mut buf[..aligned])
        }

        let nread = {
            let mut rem = self.fill_buf()?;
            rem.read(buf)?
        };
        self.consume(nread);
        Ok(nread)
    }
}

impl<R> Seek for Device<R>
where R: Read + Seek {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        let maximum = self.inner.seek(SeekFrom::Current(0))?;
        let minimum = maximum - self.cap as u64;
        let current = minimum + self.pos as u64;

        // figure out the target location
        let target = match pos {
            SeekFrom::Current(n) => { iadd(current, n) }
            SeekFrom::Start(n) => { Ok(n) }
            SeekFrom::End(n) => {
                let end = self.inner.seek(SeekFrom::End(0))?;
                self.inner.seek(SeekFrom::Start(maximum))?;
                iadd(end, n)
            }
        }?;

        if target == current {
            return Ok(target)
        }

        if target < minimum || target > maximum {
            let offset = target % self.buf.len() as u64;
            let aligned = target - offset;

            self.discard_buffer();
            if aligned != maximum {
                self.inner.seek(SeekFrom::Start(aligned))?;
            }

            if target > aligned {
                self.fill_buf()?;
                self.pos = (target - aligned) as usize;
            }
        }

        if target >= minimum && target <= maximum {
            self.pos = (target - minimum) as usize;
        }

        Ok(target)
    }
}


/// A trait for disk Volumes.
pub trait Volume<R>
where R: Read {

    /// Create a new Volume from a reader when the header has already been read.
    fn with_header(inner: R, header: &[u8; 512]) -> io::Result<Self>
    where Self: Sized;

    /// Create a new Volume from a reader.
    fn new(mut inner: R) -> io::Result<Self>
    where Self: Sized {
        let mut header: [u8; 512] = [0; 512];
        inner.read_exact(&mut header).or_else(|err| {
            eprintln!("ERROR: Read Failed: {}", err);
            Err(err)
        }).and_then(|_| {

            debug_xxd!(&header, 0);

            Self::with_header(inner, &header)
        })
    }

    /// Is the header a Volume of this type?
    fn is_supported(header: &[u8; 512]) -> bool;

    /// [re]-Load data from disk.
    fn refresh(&mut self) -> io::Result<()>;
}


#[macro_export]
macro_rules! read_struct {
    ($type:ty, $device:ident, $offset:expr, $max_size:expr) => {
        {
            let offset = $offset as u64;
            let max_size = $max_size as u64;
            if max_size < core::mem::size_of::<$type>() as u64 {
                eprintln!(
                    "ERROR: not enough space to read: {} vs {}",
                    max_size, core::mem::size_of::<$type>()
                );
                Err(std::io::Error::from(std::io::ErrorKind::InvalidData))
            } else {
                let mut raw: [u8; core::mem::size_of::<$type>()] = [0; core::mem::size_of::<$type>()];

                $device.seek(std::io::SeekFrom::Start(offset)).or_else(|err| {
                    eprintln!("ERROR: Seek Failed: {}", err);
                    Err(err)
                }).and_then(|_| {
                    $device.read_exact(&mut raw).or_else(|err| {
                        eprintln!("ERROR: Read Failed: {}", err);
                        Err(err)
                    }).and_then(|_| {

                        debug_xxd!(&raw, offset);

                        let object: $type = unsafe{ *(raw.as_ptr() as *const $type) };
                        debug!("{:#?}", object);

                        Ok(object)
                    })
                })
            }
        }
    };
}


#[macro_export]
macro_rules! read_utf16 {
    ($device:ident, $offset:expr, $length:expr, $max_size:expr) => {
        {
            let offset = $offset as u64;
            let length = $length as usize;
            let max_size = $max_size as usize;
            if max_size < length * 2 {
                eprintln!(
                    "ERROR: not enough space to read: {} vs {}",
                    max_size, length * 2
                );
                Err(std::io::Error::from(std::io::ErrorKind::InvalidData))
            } else {
                let mut raw: Vec<u16> = Vec::with_capacity(length);
                raw.resize(length, 0);
                let mut raw = raw.into_boxed_slice();
                {
                    let s: &mut [u8] = unsafe {
                        let ptr = raw.as_mut_ptr() as *mut _ as *mut u8;
                        std::slice::from_raw_parts_mut(ptr, length * 2)
                    };
                    $device.seek(std::io::SeekFrom::Start(offset))?;
                    $device.read(s)?;
                }
                String::from_utf16(&raw).or(
                    Err(std::io::Error::from(std::io::ErrorKind::InvalidData))
                )
            }
        }
    };
}