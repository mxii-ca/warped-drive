use std::fs::File;
use std::io::Result;
use std::os::unix::io::IntoRawFd;

use nix::sys::ioctl;

use super::Block;


const BLKBSZGET_TYPE: u8 = 0x12;
const BLKBSZGET_NUMBER: u8 = 112;

ioctl::ioctl!(read ioctl_blkbszget with BLKBSZGET_TYPE, BLKBSZGET_NUMBER; usize);


impl Block for File {
    fn get_block_size(&self) -> Result<usize> {
        let fd = self.into_raw_fd();
        let mut block_size: usize = 0;

        let result = ioctl_blkbszget(fd, &mut block_size);
        if let Err(err) = result {
            eprintln!("IOCTL BLKBSZGET Failed: {}\n", err);
        }
        result
    }
}