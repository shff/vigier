fn main() {
    let target = std::env::var("TARGET").unwrap();
    if target.contains("darwin") {
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-pedantic")
            .flag("-Wno-deprecated-declarations")
            .flag("-mmacosx-version-min=10.10")
            .file("src/native/macos.m")
            .compile("native.a");
        println!("cargo:rustc-link-lib=framework=Metal");
    } else if target.contains("android") {
        cc::Build::new()
            .flag("-O3")
            .flag("-Wall")
            .flag("-Wl,-s")
            .file("src/native/android.c")
            .compile("native.a");
    } else if target.contains("linux") {
        cc::Build::new()
            .flag("-O3")
            .flag("-Wall")
            .flag("-Wl,-s")
            .flag("-Wno-unused-parameter")
            .file("src/native/x11.c")
            .compile("native.a");
        println!("cargo:rustc-link-lib=X11");
        println!("cargo:rustc-link-lib=EGL");
        println!("cargo:rustc-link-lib=GL");
    } else if target.contains("x86_64-apple-ios") {
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-pedantic")
            .flag("-Wno-deprecated-declarations")
            .flag("-mios-simulator-version-min=13.0")
            .file("src/native/ios.m")
            .compile("native.a");
    } else if target.contains("aarch64-apple-ios") {
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-pedantic")
            .flag("-Wno-deprecated-declarations")
            .file("src/native/ios.m")
            .compile("native.a");
    } else if target.contains("windows") {
        cc::Build::new()
            .file("src/native/win32.c")
            .compile("native.a");
        println!("cargo:rustc-link-lib=user32");
        println!("cargo:rustc-link-lib=d3d11");
        println!("cargo:rustc-link-lib=dxguid");
        println!("cargo:rustc-link-lib=dsound");
    } else if target.contains("emscripten") {
        cc::Build::new()
            .file("src/native/emsc.c")
            .compile("native.a");
    }
}
