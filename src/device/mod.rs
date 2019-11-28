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
pub struct BlockDevice<R> {
    // ideally we would wrap this in a `BufReader` so that it can handle blocking
    // unfortunately, `BufReader.seek` always drops the buffer
    inner: R,
    buf: Box<[u8]>,
    pos: usize,
    cap: usize,
}

impl<R> BlockDevice<R> {

    /// Creates a new `BlockDevice` with the specified block size.
    pub fn with_block_size(inner: R, block_size: usize) -> BlockDevice<R> {
        let mut buffer = Vec::with_capacity(block_size);
        buffer.resize(block_size, 0);
        BlockDevice {
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

impl<R> BlockDevice<R> where R: Block {

    /// Creates a new `BlockDevice`.
    pub fn new(inner: R) -> io::Result<BlockDevice<R>> {
        let block_size = inner.get_block_size()?;
        Ok(BlockDevice::with_block_size(inner, block_size))
    }
}

impl BlockDevice<fs::File> {

    /// Open a block device or file
    pub fn open(path: &str) -> io::Result<BlockDevice<fs::File>> {
        debug!("Opening: {}", path);
        let file = fs::File::open(path)?;
        BlockDevice::new(file)
    }
}

impl<R> Block for BlockDevice<R> {
    fn get_block_size(&self) -> io::Result<usize> {
        Ok(self.buf.len())
    }
}

impl<R> BufRead for BlockDevice<R> where R: Read {
    fn fill_buf(&mut self) -> io::Result<&[u8]> {
        if self.pos >= self.cap {
            debug_assert!(self.pos == self.cap);
            debug!("[re]Populating Cache");
            self.cap = self.inner.read(&mut self.buf)?;
            self.pos = 0;
            debug!("Raw Read: Read {}", self.cap);
        }
        Ok(&self.buf[self.pos..self.cap])
    }

    fn consume(&mut self, amt: usize) {
        self.pos = cmp::min(self.pos + amt, self.cap);
    }
}

impl<R> Read for BlockDevice<R> where R: Read {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {

        // bypass the cache for large reads
        if self.pos == self.cap && buf.len() > self.buf.len() {
            let aligned = buf.len() - (buf.len() % self.buf.len());
            self.discard_buffer();
            debug!("Raw Read: Requested {} -> Aligned {}", buf.len(), aligned);
            let nread = self.inner.read(&mut buf[..aligned])?;
            debug!("Raw Read: Read {}", nread);
            return Ok(nread);
        }

        debug!("Cached Read: Requested {}", buf.len());
        let nread = {
            let mut rem = self.fill_buf()?;
            rem.read(buf)?
        };
        self.consume(nread);
        debug!("Cached Read: Read {}", nread);
        Ok(nread)
    }
}

impl<R> Seek for BlockDevice<R> where R: Read + Seek {
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
                iadd(end, n)
            }
        }?;

        debug!(
            "Cache Pos: {}\nActual Pos: {}\nVirtual Pos: {}\nTarget Pos: {}",
            self.pos, maximum, current, target
        );

        if target == current {
            return Ok(target)
        }

        if target < minimum || target > maximum {
            let offset = target % self.buf.len() as u64;
            let aligned = target - offset;

            debug!("Actual Seek: {}", aligned);

            self.discard_buffer();
            self.inner.seek(SeekFrom::Start(aligned))?;

            if target > aligned {
                self.fill_buf()?;
                debug!("Virtual Seek: {}", target - aligned);
                self.pos = (target - aligned) as usize;
            }
        }

        if target >= minimum && target <= maximum {
            debug!("Virtual Seek: {}", target as i64 - current as i64);
            self.pos = (target - minimum) as usize;
        }

        Ok(target)
    }
}


#[macro_export]
macro_rules! read_struct {
    ($type:ty, $device:ident, $offset:expr, $max_size:expr) => {
        {
            let offset = $offset as u64;
            let max_size = $max_size as u64;
            if max_size < core::mem::size_of::<$type>() as u64 {
                eprintln!(
                    "WARNING: not enough space to read: {} vs {}",
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