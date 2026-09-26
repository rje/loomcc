//! Reads testbed/loom/units.json: every Loom translation unit with the flags
//! Loom compiles it with.

use loomcc_pp::{Define, Options};
use std::path::{Path, PathBuf};

pub fn testbed_root() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../testbed")
}

pub const PVSNESLIB_INCLUDE: &str = "/Users/rje/Library/Loom/Toolchains/v0/artifacts/pvsneslib/pvsneslib/include";
pub const DEVKITSNES_INCLUDE: &str = "/Users/rje/Library/Loom/Toolchains/v0/artifacts/pvsneslib/devkitsnes/include";

#[derive(Debug, Clone)]
pub struct Unit {
    pub sample: String,
    pub profile: String,
    pub path: PathBuf,
    pub include_dirs: Vec<PathBuf>,
    pub defines: Vec<(String, Option<String>)>,
}

impl Unit {
    pub fn options(&self) -> Options {
        Options {
            include_dirs: self.include_dirs.clone(),
            defines: self.defines.iter().map(|(n, v)| Define::Set(n.clone(), v.clone())).collect(),
            target_macros: true,
            ..Default::default()
        }
    }

    pub fn name(&self) -> String {
        format!("{}/{}/{}", self.sample, self.profile, self.path.file_name().unwrap().to_string_lossy())
    }
}

/// Every (sample, profile, unit) in units.json.
pub fn loom_units() -> Vec<Unit> {
    let root = testbed_root();
    let text = match std::fs::read_to_string(root.join("loom/units.json")) {
        Ok(t) => t,
        Err(_) => return Vec::new(),
    };
    let v: serde_json::Value = serde_json::from_str(&text).expect("units.json");
    let mut out = Vec::new();
    let tc = &v["toolchain"];
    let subst = |s: &str| -> PathBuf {
        let s = s
            .replace("${PVSNESLIB_INCLUDE}", tc["PVSNESLIB_INCLUDE"].as_str().unwrap_or(PVSNESLIB_INCLUDE))
            .replace("${DEVKITSNES_INCLUDE}", tc["DEVKITSNES_INCLUDE"].as_str().unwrap_or(DEVKITSNES_INCLUDE));
        let p = PathBuf::from(&s);
        if p.is_absolute() { p } else { root.join(p) }
    };
    for (sample, s) in v["samples"].as_object().into_iter().flatten() {
        let mut includes: Vec<PathBuf> = Vec::new();
        for k in ["include_roots", "toolchain_include_roots"] {
            for r in s[k].as_array().into_iter().flatten() {
                includes.push(subst(r.as_str().unwrap()));
            }
        }
        for (profile, p) in s["profiles"].as_object().into_iter().flatten() {
            let defines: Vec<(String, Option<String>)> = p["definitions"]
                .as_array()
                .into_iter()
                .flatten()
                .map(|d| {
                    let d = d.as_str().unwrap();
                    match d.split_once('=') {
                        Some((n, v)) => (n.to_string(), Some(v.to_string())),
                        None => (d.to_string(), None),
                    }
                })
                .collect();
            for u in p["c_units"].as_array().into_iter().flatten() {
                out.push(Unit {
                    sample: sample.clone(),
                    profile: profile.clone(),
                    path: subst(u["path"].as_str().unwrap()),
                    include_dirs: includes.clone(),
                    defines: defines.clone(),
                });
            }
        }
    }
    out
}
