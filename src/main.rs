use std::collections::HashMap;
use std::env;
use std::fs::{create_dir, remove_file, File};
use std::io::Write;
use std::path::PathBuf;
use std::process::{exit, Command};

fn main() {
    if env::var("CARGO").is_err() {
        eprintln!("This binary may only be called via `cargo wrap`.");
        exit(1);
    }

    let mut run = false;
    let mut mode = "debug";
    let mut simulator = "iPhone SE (2nd generation)".to_string();
    let mut target = env::var("RUSTUP_TOOLCHAIN")
        .ok()
        .map(|toolchain| toolchain.splitn(2, '-').skip(1).collect());

    // Parse Arguments
    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match &*arg {
            "run" => run = true,
            "--release" => mode = "release",
            "--target" => target = args.next(),
            "--simulator" => simulator = args.next().expect("Simulator name expected"),
            "-h" | "--help" => {
                println!("{} v{}", env!("CARGO_PKG_NAME"), env!("CARGO_PKG_VERSION"));
                return;
            }
            "-v" | "--version" => {
                println!("{} v{}", env!("CARGO_PKG_NAME"), env!("CARGO_PKG_VERSION"));
                return;
            }
            arg => {
                eprintln!("Unknown argument: {}", arg);
            }
        }
    }

    //
    // Rust Compilation
    //

    let target = target.clone().unwrap();

    // Set Cargo Options
    let mut args = vec![];
    args.push("rustc".to_string());
    args.push("--lib".to_string());
    if mode == "release" {
        args.push("--release".to_string());
    }
    args.push(format!("--target={}", target));
    args.push("--".to_string());
    args.push("--crate-type=cdylib".to_string());

    // Android variables
    let (ndk_linker, ndk_flags) = android_path(target.clone());

    // Add Cargo environment variables
    let mut envs = HashMap::new();
    if target.contains("-linux-android") {
        let target2 = &target.replace("-", "_").to_uppercase();
        envs.insert(format!("CARGO_TARGET_{}_LINKER", target2), &ndk_linker);
    }

    // Run Cargo
    let cargo = env::var("CARGO").unwrap_or_else(|_| "cargo".into());
    if let Err(err) = Command::new(cargo.clone())
        .envs(envs)
        .args(args.clone())
        .status()
    {
        eprintln!("Error running `{} {}`: {}", cargo, args.join(" "), err);
        exit(1);
    }

    //
    // Bundle
    //

    // Paths
    let app_name = "App";
    let out_dir = env::current_dir()
        .unwrap()
        .join("target")
        .join(target.clone())
        .join(mode);

    //
    // MacOS Compilation
    //

    if target.ends_with("-darwin") {
        // Define Paths
        let bundle = out_dir.join(format!("{}.app", app_name));
        let contents = bundle.join("Contents");
        let mac_os = contents.join("MacOS");
        let output = mac_os.join(app_name);

        // Create Bundle
        create_dir(bundle.clone()).ok();
        create_dir(contents.clone()).ok();
        create_dir(mac_os.clone()).ok();

        // Create Property List
        let plist = contents.join("Info.plist");
        remove_file(plist.clone()).ok();
        let plist_keys = vec![
            format!("Add :CFBundleDisplayName string \"{}\"", app_name),
            format!("Add :CFBundleIdentifier string \"vigier.{}\"", app_name),
        ];
        write_plist(plist.clone(), plist_keys);

        // Write Wrapper to Disk
        let wrapper = include_str!("native/macos.m");
        let filename = out_dir.join("wrapper.m").display().to_string();
        let mut file = File::create(filename.clone()).expect("Can't create file");
        file.write_all(wrapper.as_bytes()).unwrap();

        // Compile
        let mut args = vec![];
        args.push("-Wall".into());
        args.push("-fmodules".into());
        args.push("-Wno-deprecated-declarations".into());
        if mode == "debug" {
            args.push("-fsanitize=undefined".into());
        }
        args.push(filename);
        args.push("-o".into());
        args.push(output.display().to_string());
        if let Err(err) = Command::new("clang").args(args.clone()).status() {
            eprintln!("Error running `clang {}`: {}", args.join(" "), err);
            exit(1);
        }

        // Sign
        if mode == "release" {
            let signature = "SHF";
            Command::new("codesign")
                .arg("-s".to_string())
                .arg(signature)
                .arg(bundle.clone())
                .status()
                .expect("Can't run codesign");
        }

        // Execute
        if run {
            Command::new("open").arg(bundle).output().ok();
        }
    }

    //
    // iOS Commpilation
    //

    if target.ends_with("-ios") {
        // Define Paths
        let bundle = out_dir.join(format!("{}.app", app_name));
        let output = bundle.join(app_name);
        let unique_id = format!("io.github.shff.ge.{}", app_name);

        // Create Bundle
        create_dir(bundle.clone()).ok();

        // Create Property List
        let plist = bundle.join("Info.plist");
        remove_file(plist.clone()).ok();
        let plist_keys = vec![
            format!("Add :CFBundleDevelopmentRegion string \"{}\"", "en"),
            format!("Add :CFBundleDisplayName string \"{}\"", app_name),
            format!("Add :CFBundleExecutable string \"{}\"", app_name),
            format!("Add :CFBundleIdentifier string \"{}\"", unique_id),
            format!("Add :CFBundleInfoDictionaryVersion string \"6.0\""),
            format!("Add :CFBundleName string \"{}\"", app_name),
            format!("Add :CFBundlePackageType string \"APPL\""),
            format!("Add :CFBundleShortVersionString string \"1.0.0\""),
            format!("Add :CFBundleSignature string \"????\""),
            format!("Add :CFBundleVersion string \"1\""),
            format!("Add :LSRequiresIPhoneOS bool \"true\""),
            format!("Add :UISupportedInterfaceOrientations.0 string UIInterfaceOrientationLandscapeLeft"),
            format!("Add :UISupportedInterfaceOrientations.1 string UIInterfaceOrientationLandscapeRight)")
        ];
        write_plist(plist, plist_keys);

        // Write Wrapper to Disk
        let wrapper = include_str!("native/ios.m");
        let filename = out_dir.join("wrapper.m").display().to_string();
        let mut file = File::create(filename.clone()).expect("Can't create file");
        file.write_all(wrapper.as_bytes()).unwrap();

        // Get SDK path
        let sdk = if run { "iphonesimulator" } else { "iphoneos" };
        let sdk_path = Command::new("xcrun")
            .arg("--show-sdk-path")
            .arg("--sdk")
            .arg(sdk)
            .output()
            .unwrap()
            .stdout;
        let sdk_path = String::from_utf8(sdk_path).unwrap().trim().to_string();

        // Compile
        let mut args = vec![];
        args.push("-Wall".into());
        args.push(filename);
        args.push("-o".into());
        args.push(output.display().to_string());
        args.push("-fmodules".into());
        args.push("-Wno-deprecated-declarations".into());
        if mode == "debug" {
            args.push("-fsanitize=undefined".into());
        }
        args.push("-fembed-bitcode".into());
        args.push("-isysroot".into());
        args.push(sdk_path);
        if run {
            args.push("--target=x86_64-apple-ios13.0-simulator".into());
        } else {
            args.push("--target=arm64-apple-ios-ios".into());
        }
        if let Err(err) = Command::new("clang").args(args.clone()).status() {
            eprintln!("Error running `clang {}`: {}", args.join(" "), err);
            exit(1);
        }

        // Execute
        if run {
            Command::new("xcrun")
                .arg("simctl")
                .arg("boot")
                .arg(simulator.clone())
                .output()
                .unwrap();
            Command::new("xcrun")
                .arg("simctl")
                .arg("install")
                .arg(simulator.clone())
                .arg(bundle)
                .output()
                .unwrap();
            Command::new("xcrun")
                .arg("simctl")
                .arg("launch")
                .arg(simulator.clone())
                .arg(unique_id)
                .output()
                .unwrap();
        }
    }

    //
    // Android Compilation
    //

    if target.contains("linux-android") {
        let output = out_dir.join(app_name.clone());

        // Write Wrapper to Disk
        let wrapper = include_str!("native/android.c");
        let filename = out_dir.join("wrapper.c").display().to_string();
        let mut file = File::create(filename.clone()).expect("Can't create file");
        file.write_all(wrapper.as_bytes()).unwrap();

        // Compile
        let mut args = vec![];
        args.push("-Wall".into());
        args.push(filename);
        args.push("-o".into());
        args.push(output.display().to_string());
        args.push("-c".into());
        args.push(ndk_flags);
        if let Err(err) = Command::new(ndk_linker.clone()).args(args.clone()).status() {
            eprintln!("Error running `{} {}`: {}", ndk_linker, args.join(" "), err);
            exit(1);
        }

        // Execute
        if run {}
    }

    //
    // X11 Compiler
    //

    if target.ends_with("-linux-gnu") {
        let output = out_dir.join(app_name.clone());

        // Write Wrapper to Disk
        let wrapper = include_str!("native/x11.c");
        let filename = out_dir.join("wrapper.c").display().to_string();
        let mut file = File::create(filename.clone()).expect("Can't create file");
        file.write_all(wrapper.as_bytes()).unwrap();

        // Compile
        let mut args = vec![];
        args.push("-Wall".into());
        args.push(filename);
        args.push("-o".into());
        args.push(output.display().to_string());
        args.push("-lX11".into());
        args.push("-lEGL".into());
        args.push("-lGL".into());
        args.push("-lasound".into());
        if let Err(err) = Command::new("cc").args(args.clone()).status() {
            eprintln!("Error running `cc {}`: {}", args.join(" "), err);
            exit(1);
        }

        // Execute
        if run {
            Command::new(output).status().expect("Can't run program");
        }
    }

    //
    // Windows Compilation
    //

    if target.ends_with("-windows-msvc") {
        let output = out_dir.join(app_name.clone());

        // Write Wrapper to Disk
        let wrapper = include_str!("native/win32.c");
        let filename = out_dir.join("wrapper.c").display().to_string();
        let mut file = File::create(filename.clone()).expect("Can't create file");
        file.write_all(wrapper.as_bytes()).unwrap();

        // Compile
        let mut args = vec![];
        args.push("-Wall".into());
        args.push(filename);
        args.push(format!("/out:{}", output.display().to_string()));
        if let Err(err) = Command::new("cl.exe").args(args.clone()).status() {
            eprintln!("Error running `cl.exe {}`: {}", args.join(" "), err);
            exit(1);
        }

        // Execute
        if run {
            Command::new(output).status().expect("Can't run program");
        }
    }
}

fn android_path(target: String) -> (String, String) {
    let ndk_target = match target.as_str() {
        "armv7-linux-androideabi" => "armv7a-linux-androideabi".to_string(),
        "arm-linux-androideabi" => "armv7a-linux-androideabi".to_string(),
        target => target.to_string(),
    };
    let platform = 30;
    let mut ndk_home = "./ndk".to_string();
    if let Some(ndk_path) = env::var_os("ANDROID_NDK_HOME") {
        ndk_home = ndk_path.to_str().unwrap().to_string();
    }
    let ndk_flags = format!("-I{}/sources/android/native_app_glue", ndk_home);
    let ndk_linker = format!(
        "{}/toolchains/llvm/prebuilt/{}/bin/{}{}-clang",
        ndk_home, &ARCH, ndk_target, platform
    );

    (ndk_linker, ndk_flags)
}

fn write_plist(path: PathBuf, commands: Vec<String>) {
    for command in commands {
        Command::new("/usr/libexec/PlistBuddy")
            .arg(path.clone())
            .arg("-c")
            .arg(command)
            .status()
            .expect("Can't write Plist file");
    }
}

#[cfg(target_os = "macos")]
const ARCH: &str = "darwin-x86_64";
#[cfg(target_os = "linux")]
const ARCH: &str = "linux-x86_64";
#[cfg(target_os = "windows")]
const ARCH: &str = "windows-x86_64";
