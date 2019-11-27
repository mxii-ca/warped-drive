#![no_std]

use winapi::shared::ntstatus::{NTSTATUS, STATUS_SUCCESS};

#[no_mangle]
pub extern "system" fn DriverEntry(driver: PVOID, path: PVOID) -> NTSTATUS {

    driver.DriverUnload = Some(DriverUnload);

    STATUS_SUCCESS
}

extern "system" fn DriverUnload(driver: PVOID) {

}