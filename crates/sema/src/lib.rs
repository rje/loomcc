//! loomcc-sema: types, 816-tcc-compatible layout, name resolution, constant
//! evaluation and implicit conversions. Produces the typed tree in [`hir`].

pub mod check;
pub mod expr;
pub mod hir;
pub mod init;
pub mod types;

pub use check::Checker;
use loomcc_parse::ast::TranslationUnit;
use loomcc_pp::Diag;

/// Type-checks a translation unit.
pub fn check(tu: &TranslationUnit, unit_name: &str, layout: types::Layout) -> (hir::Unit, Vec<Diag>) {
    let mut c = Checker::new(layout, unit_name);
    c.check_unit(tu);
    let mut file_tags: Vec<(String, types::Ty)> = c.tags[0].iter().map(|(k, v)| (k.clone(), *v)).collect();
    file_tags.sort();
    let unit = hir::Unit { types: c.types, globals: c.globals, name: unit_name.to_string(), file_tags };
    (unit, c.diags)
}

#[cfg(test)]
mod tests;
