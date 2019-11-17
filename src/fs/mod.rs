use std::io::Read;
use std::process;

use super::device::{Block, BlockDevice};
use super::utils::xxd;

mod fat;
mod ntfs;

use ntfs::parse_ntfs;


pub fn parse<R>(mut device: BlockDevice<R>) where R: Block + Read {
    let block_size = device.get_block_size().unwrap();
    eprintln!("Block size: {}\n", block_size);

    let mut header: [u8; 512] = [0; 512];
    let result = device.read_exact(&mut header);
    if let Err(err) = result {
        eprintln!("Read Failed: {}", err);
        process::exit(3);
    }
    xxd(&header, 0);

    if &header[3..7] == b"NTFS" {
        eprintln!("Filesystem: NTFS\n");
        parse_ntfs(device, &mut header);
    }
}