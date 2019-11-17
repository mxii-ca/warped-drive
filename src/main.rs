use std::env;
use std::process;

use tardis::device::BlockDevice;
use tardis::fs::parse;


fn print_usage(program: &str, err: bool) {
    if err {
        eprintln!("usage: {} [-h] device", program);
    } else {
        println!("usage: {} [-h] device", program);
    }
}

fn print_help(program: &str) {
    print_usage(program, false);
    println!("
filesystem reader

positional arguments:
  device      device or file to open

optional arguments:
  -h, --help  show this help message and exit");
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let prog = &args[0];

    for arg in args.iter() {
        if arg == "-h" || arg == "--help" {
            print_help(prog);
            process::exit(0);
        }
    }
    if args.len() < 2 {
        print_usage(prog, true);
        eprintln!("{}: error: the following arguments are required: device",
                  prog);
        process::exit(1);
    }
    if args.len() > 2 {
        print_usage(prog, true);
        eprintln!("{}: error: unrecognized arguments: {}", prog,
                  args[2..].join(" "));
        process::exit(1);
    }

    let path = &args[1];
    let result = BlockDevice::open(path);
    if let Err(err) = result {
        eprintln!("{}: error: failed to open {}: {}", prog, path, err);
        process::exit(2);
    }
    let device = result.unwrap();

    parse(device);
}
