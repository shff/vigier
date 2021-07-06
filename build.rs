fn main() {
    let target = std::env::var("TARGET").unwrap();
    if target.contains("darwin") {
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .flag("-Wno-unused-parameter")
            .flag("-mmacosx-version-min=10.10")
            .file("src/native/macos.m")
            .compile("native.a");
    } else if target.contains("x86_64-apple-ios") {
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .flag("-mios-simulator-version-min=13.0")
            .file("src/native/ios.m")
            .compile("native.a");
    } else if target.contains("aarch64-apple-ios") {
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .file("src/native/ios.m")
            .compile("native.a");
    } else if target.contains("windows") {
        cc::Build::new()
            .flag("-Wall")
            .file("src/native/win32.c")
            .compile("native.a");
        println!("cargo:rustc-link-lib=user32");
        println!("cargo:rustc-link-lib=d3d11");
        println!("cargo:rustc-link-lib=dxguid");
        println!("cargo:rustc-link-lib=dsound");
        println!("cargo:rustc-link-lib=xinput");
    } else if target.contains("android") {
        cc::Build::new()
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .file("src/native/android.c")
            .compile("native.a");
    } else if target.contains("linux") {
        cc::Build::new()
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-Wl,-s")
            .flag("-Wno-unused-parameter")
            .flag("-Wno-unused-but-set-variable")
            .file("src/native/x11.c")
            .compile("native.a");
        println!("cargo:rustc-link-lib=X11");
        println!("cargo:rustc-link-lib=EGL");
        println!("cargo:rustc-link-lib=GL");
        println!("cargo:rustc-link-lib=asound");
    }
}
