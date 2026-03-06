use std::env;
use std::path::PathBuf;

fn main() {
    let src_dir = PathBuf::from("../src");
    let include_dir = PathBuf::from("../include");

    println!("cargo:rerun-if-changed=../src");
    println!("cargo:rerun-if-changed=../include");

    // Build the C files into a static library
    cc::Build::new()
        .include(&include_dir)
        .file(src_dir.join("core/discovery.c"))
        .file(src_dir.join("core/dns_resolver.c"))
        .compile("blackmap_c_engines");
}
