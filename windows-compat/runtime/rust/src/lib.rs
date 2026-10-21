//! Operating-system identity must not be inferred from the executable bitness,
//! browser user agent, compatibility shims, or a manifest-dependent API.

#[cfg(feature="contracts")]
pub mod canonical;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WindowsIdentity {
    pub version: [u32; 3],
    pub service_pack: u16,
    pub native_architecture: &'static str,
    pub process_architecture: &'static str,
}

pub fn parse_version(value: &str) -> Result<[u32; 3], &'static str> {
    let parts: Vec<_> = value.split('.').collect();
    if parts.len() != 3 { return Err("Windows version is incomplete"); }
    let mut result = [0; 3];
    for (index, part) in parts.iter().enumerate() {
        if part.is_empty() || !part.bytes().all(|ch| ch.is_ascii_digit()) { return Err("invalid Windows version"); }
        result[index] = part.parse().map_err(|_| "invalid Windows version")?;
    }
    Ok(result)
}

pub fn system_id(version: [u32; 3], native_architecture: &str) -> Result<String, &'static str> {
    if !matches!(native_architecture, "x86" | "x86_64") { return Err("unsupported native architecture"); }
    let family = match version {
        [6, 1, build] if build >= 7601 => "win7-sp1-original",
        [6, 2, build] if build >= 9200 => "win8",
        [6, 3, build] if build >= 9600 => "win81",
        [10, 0, build] if build >= 22000 && native_architecture == "x86_64" => "win11",
        [10, 0, build] if (10240..22000).contains(&build) => "win10",
        _ => return Err("unsupported Windows version"),
    };
    Ok(format!("{family}-{native_architecture}"))
}

impl WindowsIdentity {
    pub fn version_string(&self) -> String { format!("{}.{}.{}", self.version[0], self.version[1], self.version[2]) }
    pub fn system_id(&self) -> Result<String, &'static str> {
        if self.version[0..2] == [6, 1] && self.service_pack < 1 { return Err("Windows 7 SP1 is required"); }
        system_id(self.version, self.native_architecture)
    }
    pub fn default_profile(&self) -> Result<&'static str, &'static str> {
        self.system_id()?;
        Ok(if self.version[0] == 10 { "modern" } else { "legacy" })
    }
}

#[cfg(windows)]
pub fn detect() -> Result<WindowsIdentity, &'static str> {
    use std::ffi::c_void;
    #[repr(C)]
    struct VersionInfo {
        size: u32, major: u32, minor: u32, build: u32, platform: u32,
        description: [u16; 128], sp_major: u16, sp_minor: u16,
        suite: u16, product_type: u8, reserved: u8,
    }
    #[repr(C)]
    struct SystemInfo {
        architecture: u16, reserved: u16, page_size: u32,
        minimum_address: *mut c_void, maximum_address: *mut c_void,
        active_processor_mask: usize, processor_count: u32, processor_type: u32,
        allocation_granularity: u32, processor_level: u16, processor_revision: u16,
    }
    #[link(name = "ntdll")]
    unsafe extern "system" { fn RtlGetVersion(info: *mut VersionInfo) -> i32; }
    #[link(name = "kernel32")]
    unsafe extern "system" { fn GetNativeSystemInfo(info: *mut SystemInfo); }
    // These C structures contain only integers and raw pointers; zero is valid.
    let mut version: VersionInfo = unsafe { std::mem::zeroed() };
    version.size = std::mem::size_of::<VersionInfo>() as u32;
    if unsafe { RtlGetVersion(&mut version) } < 0 { return Err("RtlGetVersion failed"); }
    let mut system: SystemInfo = unsafe { std::mem::zeroed() };
    unsafe { GetNativeSystemInfo(&mut system) };
    let identity = WindowsIdentity {
        version: [version.major, version.minor, version.build], service_pack: version.sp_major,
        native_architecture: match system.architecture { 0 => "x86", 9 => "x86_64", _ => return Err("unsupported native architecture") },
        process_architecture: if cfg!(target_arch = "x86") { "x86" } else if cfg!(target_arch = "x86_64") { "x86_64" } else { return Err("unsupported process architecture") },
    };
    identity.system_id()?;
    Ok(identity)
}

#[cfg(not(windows))]
pub fn detect() -> Result<WindowsIdentity, &'static str> { Err("Windows identity is unavailable on this operating system") }

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn never_guess_system_or_native_architecture() {
        for version in [[6,0,6002],[6,1,7600],[6,2,0],[10,0,0],[11,0,1]] {
            assert!(system_id(version,"x86").is_err());
        }
        assert!(system_id([6,1,7601], "arm64").is_err());
        assert!(parse_version("6.1.7601.0").is_err());
        assert!(parse_version("+6.1.7601").is_err());
    }
    #[test]
    fn wow64_is_not_a_32_bit_os() {
        let identity = WindowsIdentity { version:[6,1,7601], service_pack:1, native_architecture:"x86_64", process_architecture:"x86" };
        assert_eq!(identity.system_id(),Ok("win7-sp1-original-x86_64".into()));
        assert_eq!(identity.default_profile(),Ok("legacy"));
    }
}
