use std::io;


pub fn iadd(lvalue: u64, rvalue: i64) -> io::Result<u64> {
    let result = if rvalue > 0 {
        lvalue.checked_add(rvalue as u64)
    } else {
        lvalue.checked_sub((-rvalue) as u64)
    };
    if let Some(value) = result {
        Ok(value)
    } else {
        Err(io::Error::from(io::ErrorKind::InvalidInput))
    }
}


pub fn xxd(buffer: &[u8], address: u64) {
    let size = buffer.len();
    for i in (0..size).step_by(16) {
        eprint!("{:08x}:", address + i as u64);

        let mut j: usize = 0;
        while j < 8 && i + j < size {
            eprint!(" {:02x}", buffer[i + j]);
            j += 1;
        }
        if i + j < size {
            eprint!(" -");
        } else {
            eprint!("  ");
        }
        while j < 16 && i + j < size {
            eprint!(" {:02x}", buffer[i + j]);
            j += 1;
        }
        while j < 16 {
            eprint!("   ");
            j += 1;
        }

        eprint!("  ");

        j = 0;
        while j < 16 && i + j < size {
            if (buffer[i + j] < 32) || (buffer[i + j] > 126) {
                eprint!(".");
            } else {
                eprint!("{}", buffer[i + j] as char);
            }
            j += 1;
        }

        eprintln!("");
    }
    eprintln!("");
}


#[macro_export]
macro_rules! debug {
    ($($arg:tt)*) => {
        #[cfg(debug_assertions)]
        eprintln!("{}\n", format_args!($($arg)*));
    }
}


#[macro_export]
macro_rules! debug_xxd {
    ($buffer:expr, $address:expr) => {
        #[cfg(debug_assertions)]
        $crate::utils::xxd($buffer, $address);
    }
}