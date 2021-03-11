use std::env;
use std::error::Error;
use std::fs::{create_dir, remove_file, File};
use std::io::Write;
use std::process::Command;

fn main() {
    if let Err(err) = run() {
        eprintln!("Error: {}", err);
    }
}

fn run() -> Result<(), Box<dyn Error>> {
    let mut run = false;
    let mut mode = "debug";
    let mut platform = PLATFORM;
    let mut signature = "SHF".to_string();
    let mut simulator = "iPhone SE (2nd generation)".to_string();

    // Parse Arguments
    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match &*arg {
            "run" => run = true,
            "macos" => platform = "macos",
            "ios" => platform = "ios",
            "win" => platform = "win",
            "android" => platform = "android",
            "x11" => platform = "x11",
            "web" => platform = "web",
            "--release" => mode = "release",
            "--simulator" => simulator = args.next().ok_or("Simulator name expected")?,
            "--signature" => signature = args.next().ok_or("Signature name expected")?,
            "-h" | "--help" => {
                println!("{} v{}", env!("CARGO_PKG_NAME"), env!("CARGO_PKG_VERSION"));
                return Ok(());
            }
            "-v" | "--version" => {
                println!("{} v{}", env!("CARGO_PKG_NAME"), env!("CARGO_PKG_VERSION"));
                return Ok(());
            }
            arg => {
                eprintln!("Unknown argument: {}", arg);
            }
        }
    }

    //
    // Bundle
    //

    // Paths
    let app_name = "App";
    let current_dir = env::current_dir()?;
    let target_dir = current_dir.join("target");
    let out_dir = target_dir.join(platform);
    create_dir(target_dir).ok();
    create_dir(out_dir.clone()).ok();

    //
    // MacOS Compilation
    //

    if platform == "macos" {
        // Define Paths
        let bundle = out_dir.join(format!("{}.app", app_name));
        let contents = bundle.join("Contents");
        let mac_os = contents.join("MacOS");
        let output = mac_os.join(app_name);

        // Create Bundle
        create_dir(bundle.clone()).ok();
        create_dir(contents.clone()).ok();
        create_dir(mac_os).ok();

        // Create Property List
        let plist = contents.join("Info.plist");
        remove_file(plist.clone()).ok();
        let plist_keys = vec![
            format!("Add :CFBundleDisplayName string \"{}\"", app_name),
            format!("Add :CFBundleIdentifier string \"{}\"", app_name),
        ];
        for key in plist_keys {
            Command::new("/usr/libexec/PlistBuddy")
                .arg(plist.clone())
                .arg("-c")
                .arg(key)
                .status()?;
        }

        // Write Wrapper to Disk
        let wrapper = include_str!("native/macos.m");
        let filename = out_dir.join("wrapper.m").display().to_string();
        let mut file = File::create(filename.clone())?;
        file.write_all(wrapper.as_bytes())?;

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
        Command::new("clang").args(args).status()?;

        // Sign
        if mode == "release" {
            Command::new("codesign")
                .arg("-s".to_string())
                .arg(signature.clone())
                .arg(bundle.clone())
                .status()?;
        }

        // Execute
        if run {
            Command::new("open").arg(bundle).output().ok();
        }
    }

    //
    // iOS Commpilation
    //

    if platform == "ios" {
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
            format!("Add :CFBundleName string \"{}\"", app_name),
            "Add :CFBundleInfoDictionaryVersion string \"6.0\"".into(),
            "Add :CFBundlePackageType string \"APPL\"".into(),
            "Add :CFBundleShortVersionString string \"1.0.0\"".into(),
            "Add :CFBundleSignature string \"????\"".into(),
            "Add :CFBundleVersion string \"1\"".into(),
            "Add :LSRequiresIPhoneOS bool \"true\"".into(),
            "Add :UISupportedInterfaceOrientations.0 string UIInterfaceOrientationLandscapeLeft"
                .into(),
            "Add :UISupportedInterfaceOrientations.1 string UIInterfaceOrientationLandscapeRight"
                .into(),
        ];
        for key in plist_keys {
            Command::new("/usr/libexec/PlistBuddy")
                .arg(plist.clone())
                .arg("-c")
                .arg(key)
                .status()?;
        }

        // Write Wrapper to Disk
        let wrapper = include_str!("native/ios.m");
        let filename = out_dir.join("wrapper.m").display().to_string();
        let mut file = File::create(filename.clone())?;
        file.write_all(wrapper.as_bytes())?;

        // Get SDK path
        let sdk = if run { "iphonesimulator" } else { "iphoneos" };
        let sdk_path = Command::new("xcrun")
            .arg("--show-sdk-path")
            .arg("--sdk")
            .arg(sdk)
            .output()?
            .stdout;
        let sdk_path = String::from_utf8(sdk_path)?.trim().to_string();

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
        Command::new("clang").args(args).status()?;

        // Sign
        if mode == "release" {
            Command::new("codesign")
                .arg("-s".to_string())
                .arg(signature)
                .arg(bundle.clone())
                .output()?;
        }

        // Execute
        if run {
            Command::new("xcrun")
                .arg("simctl")
                .arg("boot")
                .arg(simulator.clone())
                .output()?;
            Command::new("xcrun")
                .arg("simctl")
                .arg("install")
                .arg(simulator.clone())
                .arg(bundle)
                .output()?;
            Command::new("xcrun")
                .arg("simctl")
                .arg("launch")
                .arg(simulator)
                .arg(unique_id)
                .output()?;
        }
    }

    //
    // Android Compilation
    //

    if platform == "android" {
        let output = out_dir.join(app_name);
        let ndk_target = "armv7a-linux-androideabi".to_string();

        // Get SDK Path
        let ndk_platform = 30;
        let ndk_home =
            env::var_os("ANDROID_NDK_HOME").ok_or("ANDROID_NDK_HOME variable required")?;
        let ndk_flags = format!(
            "-I{}/sources/android/native_app_glue",
            ndk_home.to_str().unwrap()
        );
        let ndk_linker = format!(
            "{}/toolchains/llvm/prebuilt/{}/bin/{}{}-clang",
            ndk_home.to_str().unwrap(),
            &ARCH,
            ndk_target,
            ndk_platform
        );

        // Write Wrapper to Disk
        let wrapper = include_str!("native/android.c");
        let filename = out_dir.join("wrapper.c").display().to_string();
        let mut file = File::create(filename.clone())?;
        file.write_all(wrapper.as_bytes())?;

        // Compile
        let mut args = vec![];
        args.push("-Wall".into());
        args.push(filename);
        args.push("-o".into());
        args.push(output.display().to_string());
        args.push("-c".into());
        args.push(ndk_flags);
        Command::new(ndk_linker).args(args).status()?;

        // Execute
        if run {}
    }

    //
    // X11 Compiler
    //

    if platform == "x11" {
        let output = out_dir.join(app_name);

        // Write Wrapper to Disk
        let wrapper = include_str!("native/x11.c");
        let filename = out_dir.join("wrapper.c").display().to_string();
        let mut file = File::create(filename.clone())?;
        file.write_all(wrapper.as_bytes())?;

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
        Command::new("cc").args(args).status()?;

        // Execute
        if run {
            Command::new(output).status()?;
        }
    }

    //
    // Windows Compilation
    //

    if platform == "win" {
        let output = out_dir.join(format!("{}.exe", app_name));

        // Write Wrapper to Disk
        let wrapper = include_str!("native/win32.c");
        let filename = out_dir.join("wrapper.c");
        let mut file = File::create(filename.clone())?;
        file.write_all(wrapper.as_bytes())?;

        // Compile
        let mut args = vec![];
        args.push(filename.display().to_string());
        args.push("-o".to_string());
        args.push(output.display().to_string());
        Command::new("clang").args(args).status()?;

        // Execute
        if run {
            Command::new(output).status()?;
        }
    }

    //
    // WASM Compilation
    //

    if platform == "web" {
        let _output = out_dir.join(format!("{}.wasm", app_name));

        // Write Wrapper to Disk
        let wrapper = include_str!("native/web.html");
        let filename = out_dir.join(format!("{}.html", app_name));
        let mut file = File::create(filename.clone())?;
        file.write_all(wrapper.as_bytes())?;
    }

    Ok(())
}

#[cfg(target_os = "macos")]
const ARCH: &str = "darwin-x86_64";
#[cfg(target_os = "linux")]
const ARCH: &str = "linux-x86_64";
#[cfg(target_os = "windows")]
const ARCH: &str = "windows-x86_64";

#[cfg(target_os = "macos")]
const PLATFORM: &str = "macos";
#[cfg(target_os = "linux")]
const PLATFORM: &str = "x11";
#[cfg(target_os = "windows")]
const PLATFORM: &str = "win32";
