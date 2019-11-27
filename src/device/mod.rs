use std::cmp;
use std::fs;
use std::io;
use std::io::{BufRead, BufReader, Read, Seek, SeekFrom};

#[cfg_attr(unix, path = "unix_um.rs")]
#[cfg_attr(all(windows, target_mode = "kernel"), path = "windows_km.rs")]
#[cfg_attr(all(windows, not(target_mode = "kernel")), path = "windows_um.rs")]
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
    inner: BufReader<R>,
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
        debug!("Opening: {}", path);

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