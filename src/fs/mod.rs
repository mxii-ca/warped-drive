use std::io;
use std::io::{Read, Seek};

use super::device::{Block, Device, Volume};

mod fat;
mod ntfs;

pub use ntfs::Ntfs;


pub fn parse<R>(mut device: Device<R>) -> io::Result<impl Volume<Device<R>>>
where R: Block + Read + Seek {
    let block_size = device.get_block_size()?;
    debug!("Block size: {}", block_size);

    let mut header: [u8; 512] = [0; 512];
    device.read_exact(&mut header).or_else(|err| {
        eprintln!("ERROR: Read Failed: {}", err);
        Err(err)
    }).and_then(|_| {

        debug_xxd!(&header, 0);

        if Ntfs::<R>::is_supported(&header) {
            debug!("Filesystem: NTFS");
            Ntfs::with_header(device, &mut header)
        } else {
            Err(io::Error::from(io::ErrorKind::NotFound))
        }
    })
}