use std::io;
use std::io::{Read, Seek};

use super::device::{Block, BlockDevice};

mod fat;
mod ntfs;

use ntfs::parse_ntfs;


pub fn parse<R>(mut device: BlockDevice<R>) -> io::Result<()> where R: Block + Read + Seek {
    device.get_block_size().and_then(|block_size| {
        debug!("Block size: {}", block_size);

        let mut header: [u8; 512] = [0; 512];
        device.read_exact(&mut header).or_else(|err| {
            eprintln!("ERROR: Read Failed: {}", err);
            Err(err)
        }).and_then(|_| {

            debug_xxd!(&header, 0);

            if &header[3..7] == b"NTFS" {
                debug!("Filesystem: NTFS");
                parse_ntfs(device, &mut header)
            } else {
                Ok(())
            }
        })
    })
}