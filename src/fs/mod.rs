use std::io::{Read, Seek};
use std::process;

use super::device::{Block, BlockDevice};

mod fat;
mod ntfs;

use ntfs::parse_ntfs;


pub fn parse<R>(mut device: BlockDevice<R>) where R: Block + Read + Seek {
    let block_size = device.get_block_size().unwrap();
    debug!("Block size: {}", block_size);

    let mut header: [u8; 512] = [0; 512];
    let result = device.read_exact(&mut header);
    if let Err(err) = result {
        eprintln!("ERROR: Read Failed: {}", err);
        process::exit(3);
    }
    debug_xxd!(&header, 0);

    if &header[3..7] == b"NTFS" {
        debug!("Filesystem: NTFS");
        parse_ntfs(device, &mut header);
    }
}