//! Locating loomcc and the reference tools.

use std::path::{Path, PathBuf};

#[derive(Default)]
pub struct Overrides {
    pub pvsneslib: Option<PathBuf>,
    pub emulator: Option<PathBuf>,
    pub llvm_bin: Option<PathBuf>,
    pub clang: Option<PathBuf>,
}

pub struct Tools {
    pub loomcc: PathBuf,
    /// Host clang (Apple clang is fine): `clang -E -P`, host execution.
    pub clang: Option<PathBuf>,
    /// An LLVM clang whose IR `lli` can read (msp430 target, 16-bit int).
    pub llvm_clang: Option<PathBuf>,
    pub lli: Option<PathBuf>,
    pub llvm_link: Option<PathBuf>,
    pub llvm_opt: Option<PathBuf>,
    pub tcc: Option<PathBuf>,
    pub tcc_opt: Option<PathBuf>,
    pub wla: Option<PathBuf>,
    pub wlalink: Option<PathBuf>,
    pub pvsneslib: Option<PathBuf>,
    pub emulator: Option<PathBuf>,
    /// harness/include: loomcc-test.h and friends, passed as -I to every tool.
    pub harness_include: PathBuf,
    pub harness_rom: PathBuf,
}

fn exists(p: PathBuf) -> Option<PathBuf> {
    if p.exists() {
        Some(p)
    } else {
        None
    }
}

fn which(name: &str) -> Option<PathBuf> {
    let path = std::env::var_os("PATH")?;
    std::env::split_paths(&path).map(|d| d.join(name)).find(|p| p.is_file())
}

impl Tools {
    pub fn discover(root: &Path, loomcc: PathBuf, o: Overrides) -> Tools {
        let pvs = o
            .pvsneslib
            .or_else(|| std::env::var_os("PVSNESLIB_HOME").map(PathBuf::from))
            .unwrap_or_else(|| {
                // Where Loom installs its toolchain.
                let home = std::env::var_os("HOME").map(PathBuf::from).unwrap_or_default();
                home.join("Library/Loom/Toolchains/v0/artifacts/pvsneslib")
            });
        let pvs = exists(pvs);
        let bin = pvs.as_ref().map(|p| p.join("devkitsnes/bin"));
        let llvm = o
            .llvm_bin
            .or_else(|| std::env::var_os("LLVM_BIN").map(PathBuf::from))
            .unwrap_or_else(|| PathBuf::from("/opt/homebrew/opt/llvm/bin"));
        let emulator = o
            .emulator
            .or_else(|| std::env::var_os("LOOM_EMULATOR").map(PathBuf::from))
            .unwrap_or_else(|| {
                // A Loom checkout: $LOOM_REPO, else a sibling of the loomcc repository.
                let loom = std::env::var_os("LOOM_REPO").map(PathBuf::from).unwrap_or_else(|| root.join("../../loom"));
                loom.join("target/debug/loom-emulator")
            });
        Tools {
            loomcc,
            clang: o.clang.or_else(|| which("clang")),
            llvm_clang: exists(llvm.join("clang")),
            lli: exists(llvm.join("lli")),
            llvm_link: exists(llvm.join("llvm-link")),
            llvm_opt: exists(llvm.join("opt")),
            tcc: bin.as_ref().and_then(|b| exists(b.join("816-tcc"))),
            tcc_opt: pvs.as_ref().and_then(|p| exists(p.join("devkitsnes/tools/816-opt"))),
            wla: bin.as_ref().and_then(|b| exists(b.join("wla-65816"))),
            wlalink: bin.as_ref().and_then(|b| exists(b.join("wlalink"))),
            pvsneslib: pvs,
            emulator: exists(emulator),
            harness_include: root.join("harness/include"),
            harness_rom: root.join("harness/rom"),
        }
    }

    /// 816-tcc's own include directories.
    pub fn tcc_includes(&self) -> Vec<String> {
        match &self.pvsneslib {
            Some(p) => vec![
                format!("-I{}", p.join("pvsneslib/include").display()),
                format!("-I{}", p.join("devkitsnes/include").display()),
            ],
            None => Vec::new(),
        }
    }

    pub fn rom_ready(&self) -> bool {
        self.wla.is_some() && self.wlalink.is_some() && self.pvsneslib.is_some() && self.emulator.is_some()
    }

    pub fn describe(&self) -> String {
        let f = |name: &str, p: &Option<PathBuf>| format!("{} {}", name, if p.is_some() { "yes" } else { "no" });
        [
            f("clang", &self.clang),
            f("clang16(msp430)", &self.llvm_clang),
            f("lli", &self.lli),
            f("816-tcc", &self.tcc),
            f("wla", &self.wla),
            f("emulator", &self.emulator),
        ]
        .join(", ")
    }
}
