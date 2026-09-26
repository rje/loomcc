//! loomcc-ir: the IR, lowering from the typed tree, and an interpreter.

pub mod interp;
pub mod ir;
pub mod lower;
pub mod verify;

pub use ir::*;

#[cfg(test)]
mod tests;
