use std::fs;
use std::io;
use std::mem;
use std::os::windows::io::AsRawHandle;
use std::ptr;

use winapi::shared::minwindef::{DWORD, ULONG};
use winapi::shared::ntdef::PVOID;
use winapi::shared::winerror;
use winapi::um::errhandlingapi;
use winapi::um::ioapiset;
use winapi::um::winioctl;
use winapi::um::winnt::HANDLE;

use super::Block;


winapi::STRUCT! {
    #[allow(non_snake_case)]
    #[derive(Debug)]
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

winapi::STRUCT! {
    #[allow(non_snake_case)]
    #[derive(Debug)]
    struct DISK_GEOMETRY {
        Cylinders: u64,  // LARGE_INTEGER
        MediaType: winioctl::MEDIA_TYPE,
        TracksPerCylinder: DWORD,
        SectorsPerTrack: DWORD,
        BytesPerSector: DWORD,
    }
}


impl Block for fs::File {
    fn get_block_size(&self) -> io::Result<usize> {
        let mut query = winioctl::STORAGE_PROPERTY_QUERY {
            PropertyId: winioctl::StorageAccessAlignmentProperty,
            QueryType: winioctl::PropertyStandardQuery,
            AdditionalParameters: [0]
        };
        let mut alignment: STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR = unsafe { mem::zeroed() };
        let mut size = 0;
        let ret = unsafe {
            ioapiset::DeviceIoControl(
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
            let err = io::Error::from_raw_os_error(unsafe{ errhandlingapi::GetLastError() } as i32);
            eprintln!("IOCTL_STORAGE_QUERY_PROPERTY Failed: {}\n", err);

            // IOCTL_STORAGE_QUERY_PROPERTY doesn't work for external drives
            // fallback to the logical sector size
            let mut geometry : DISK_GEOMETRY = unsafe { mem::zeroed() };
            let ret = unsafe {
                ioapiset::DeviceIoControl(
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
                let err = io::Error::from_raw_os_error(unsafe{ errhandlingapi::GetLastError() } as i32);
                eprintln!("IOCTL_DISK_GET_DRIVE_GEOMETRY Failed: {}\n", err);
                Err(err)
            } else {
                debug!("{:#?}", &geometry);
                Ok(geometry.BytesPerSector as usize)
            }
        } else {
            debug!("{:#?}", &alignment);
            Ok(alignment.BytesPerPhysicalSector as usize)
        }
    }
}