//! Widen once, scale once.
//!
//! Lowering widens an 8-bit value at every use (`zext` per use) and every
//! memory access scales its index itself. For a pool slot index used across
//! a large body (Loom's actor update: one u8 slot, dozens of `pool[i]`
//! accesses of 1-, 2- and 4-byte arrays) the code generator then reloads,
//! masks and shifts the index before each access.
//!
//! For a register defined once: its `zext`s become one, computed where
//! every use sees it (see `placement`); and an index scale used at least
//! twice becomes one shifted copy, so accesses index with scale 1 (which
//! the code generator turns into `lda array,x` with X loaded once, or a
//! register home). A compare of two widened bytes compares the bytes
//! (`narrow_compares`).

use loomcc_ir::*;
use std::collections::HashMap;

/// Where each register is defined: (block, index of the defining
/// instruction), for registers defined exactly once; parameters are
/// defined at the start of the entry block (index None).
fn single_defs(f: &Func) -> HashMap<VReg, (usize, Option<usize>)> {
    let mut count: HashMap<VReg, u32> = HashMap::new();
    let mut at: HashMap<VReg, (usize, Option<usize>)> = HashMap::new();
    for r in f.param_regs.iter().flatten() {
        *count.entry(*r).or_default() += 1;
        at.insert(*r, (0, None));
    }
    for (bi, b) in f.blocks.iter().enumerate() {
        for (k, i) in b.insts.iter().enumerate() {
            if let Some(d) = i.def() {
                *count.entry(d).or_default() += 1;
                at.insert(d, (bi, Some(k)));
            }
        }
    }
    at.retain(|r, _| count.get(r) == Some(&1));
    at
}

/// Is the point (block, index) dominated by the definition at `def`? A
/// parameter (index None) is defined before everything reachable.
fn def_dominates(idom: &[Option<usize>], def: (usize, Option<usize>), (bi, k): (usize, usize)) -> bool {
    match def {
        (db, Some(di)) if db == bi => k > di,
        (db, _) => crate::copy::dominates(idom, db, bi) && idom[bi].is_some(),
    }
}

/// Where to compute a value derived from a register defined at `def` and
/// used at `uses` (block, instruction index): the nearest common dominator
/// of the uses, so a path that never uses it does not pay for it (an early
/// return, say); right after the definition when that is the defining
/// block itself.
///
/// The common dominator is only safe when the definition dominates every
/// use: then each path from a redefinition (a loop around the definition)
/// to a use passes the common dominator again, which recomputes the value.
/// A use reached before the definition (through a loop's back edge, the
/// previous iteration's value) could otherwise see a copy computed before
/// the register changed; those registers get the copy right after the
/// definition, which follows the register exactly.
fn placement(idom: &[Option<usize>], def: (usize, Option<usize>), uses: &[(usize, usize)]) -> (usize, Option<usize>) {
    if uses.is_empty() || !uses.iter().all(|&u| def_dominates(idom, def, u)) {
        return def;
    }
    let depth = |mut b: usize| {
        let mut d = 0;
        while b != 0 {
            match idom[b] {
                Some(p) if p != b => b = p,
                _ => break,
            }
            d += 1;
        }
        d
    };
    let mut l = uses[0].0;
    for &(u, _) in &uses[1..] {
        let (mut a, mut b) = (l, u);
        while a != b {
            if depth(a) >= depth(b) {
                a = idom[a].unwrap_or(0);
            } else {
                b = idom[b].unwrap_or(0);
            }
        }
        l = a;
    }
    if l == def.0 {
        def
    } else {
        // Insert at the start of `l` (index None means "before instruction 0").
        (l, None)
    }
}

/// Inserts `insts` right after the definition point.
fn insert_after(f: &mut Func, (bi, k): (usize, Option<usize>), insts: Vec<Inst>) {
    let pos = k.map_or(0, |k| k + 1);
    let b = &mut f.blocks[bi].insts;
    for (n, i) in insts.into_iter().enumerate() {
        b.insert(pos + n, i);
    }
}

/// `recursive`: the function will have a frame on the hardware stack,
/// whose prologue masks byte parameters: each widening of a parameter is
/// then free, and one shared copy would only cost a move and a word.
pub fn widen_and_scale(f: &mut Func, recursive: bool) -> bool {
    let mut changed = false;
    // 1. One zext per 8-bit register.
    let defs = single_defs(f);
    let mut zexts: HashMap<VReg, u32> = HashMap::new();
    for b in &f.blocks {
        for i in &b.insts {
            if let Inst::Conv { kind: ConvKind::Zext, src: Operand::Reg(r), from: IrTy::I8, dst } = i {
                if f.ty(*dst) == IrTy::I16 && defs.contains_key(r) {
                    *zexts.entry(*r).or_default() += 1;
                }
            }
        }
    }
    let mut wide: HashMap<VReg, VReg> = HashMap::new();
    // A parameter costs a copy into a new home where a local's widening
    // can share its home: widen it once only when used widened often
    // enough to pay for that, and never when the prologue cleans it.
    let params: std::collections::HashSet<VReg> = f.param_regs.iter().flatten().copied().collect();
    let enough = |r: &VReg, n: u32| match params.contains(r) {
        true => !recursive && n >= 4,
        false => n >= 2,
    };
    let mut order: Vec<VReg> = zexts.iter().filter(|(r, &n)| enough(r, n)).map(|(r, _)| *r).collect();
    order.sort();
    for r in order {
        let w = f.new_vreg(IrTy::I16);
        wide.insert(r, w);
    }
    if !wide.is_empty() {
        for b in &mut f.blocks {
            for i in &mut b.insts {
                if let Inst::Conv { kind: ConvKind::Zext, src: Operand::Reg(r), from: IrTy::I8, dst } = i {
                    if let Some(&w) = wide.get(r) {
                        if f.vregs[dst.0 as usize] == IrTy::I16 {
                            *i = Inst::Mov { dst: *dst, src: Operand::Reg(w) };
                        }
                    }
                }
            }
        }
        // Definitions last to first within a block, so indices stay valid.
        let mut use_at: HashMap<VReg, Vec<(usize, usize)>> = HashMap::new();
        for (bi, b) in f.blocks.iter().enumerate() {
            for (k, i) in b.insts.iter().enumerate() {
                if let Inst::Mov { src: Operand::Reg(w), .. } = i {
                    use_at.entry(*w).or_default().push((bi, k));
                }
            }
        }
        let idom = crate::copy::dominators(f);
        let mut places: Vec<((usize, Option<usize>), VReg, VReg)> =
            wide.iter().map(|(r, w)| (placement(&idom, defs[r], use_at.get(w).map_or(&[][..], |v| &v[..])), *r, *w)).collect();
        // Last position first, so earlier positions stay valid; ties (two
        // copies at the start of one block) in register order, so the
        // output does not depend on hash order.
        places.sort_by(|a, b| b.0.cmp(&a.0).then(b.2.cmp(&a.2)));
        for (at, r, w) in places {
            insert_after(f, at, vec![Inst::Conv { kind: ConvKind::Zext, dst: w, src: Operand::Reg(r), from: IrTy::I8 }]);
        }
        changed = true;
    }
    changed
}

/// 2. One shifted copy per (index register, scale) used at least twice.
/// Run after copy propagation, so the per-use copies of the widened value
/// have become the value itself.
pub fn scale_once(f: &mut Func) -> bool {
    let defs = single_defs(f);
    let mut uses: HashMap<(VReg, u32), u32> = HashMap::new();
    let visit = |a: &Addr, uses: &mut HashMap<(VReg, u32), u32>| {
        if let Some((r, s)) = a.index {
            if s > 1 && s.is_power_of_two() && defs.contains_key(&r) {
                *uses.entry((r, s)).or_default() += 1;
            }
        }
    };
    for b in &f.blocks {
        for i in &b.insts {
            match i {
                Inst::Load { addr, .. } | Inst::Store { addr, .. } | Inst::Lea { addr, .. } => visit(addr, &mut uses),
                _ => {}
            }
        }
    }
    let mut keys: Vec<(VReg, u32)> = uses.iter().filter(|(k, &n)| n >= 2 && f.ty(k.0) == IrTy::I16).map(|(k, _)| *k).collect();
    if keys.is_empty() {
        return false;
    }
    keys.sort();
    let mut scaled: HashMap<(VReg, u32), VReg> = HashMap::new();
    for k in &keys {
        let t = f.new_vreg(IrTy::I16);
        scaled.insert(*k, t);
    }
    let mut use_at: HashMap<VReg, Vec<(usize, usize)>> = HashMap::new();
    for (bi, b) in f.blocks.iter_mut().enumerate() {
        for (k, i) in b.insts.iter_mut().enumerate() {
            let addr = match i {
                Inst::Load { addr, .. } | Inst::Store { addr, .. } | Inst::Lea { addr, .. } => addr,
                _ => continue,
            };
            if let Some((r, s)) = addr.index {
                if let Some(&t) = scaled.get(&(r, s)) {
                    addr.index = Some((t, 1));
                    use_at.entry(t).or_default().push((bi, k));
                }
            }
        }
    }
    let idom = crate::copy::dominators(f);
    let mut places: Vec<((usize, Option<usize>), VReg, u32, VReg)> = scaled
        .iter()
        .map(|((r, s), t)| (placement(&idom, defs[r], use_at.get(t).map_or(&[][..], |v| &v[..])), *r, *s, *t))
        .collect();
    places.sort_by(|a, b| b.0.cmp(&a.0).then(b.3.cmp(&a.3)));
    for (at, r, s, t) in places {
        insert_after(
            f,
            at,
            vec![Inst::Bin { op: BinOp::Shl, dst: t, a: Operand::Reg(r), b: Operand::Imm(s.trailing_zeros() as i64) }],
        );
    }
    true
}

/// 3. `cmp i16 (zext a), (zext b)` compares the bytes themselves: `cmp i8
/// a, b` lets the code generator choose between a 16-bit compare of
/// known-clean values and an 8-bit one, instead of masking both sides.
/// Only widenings used by nothing else are dropped this way. Sign
/// extensions narrow for equality and signed conditions, zero extensions
/// for equality and unsigned conditions (and for signed ones, whose
/// operands are then both non-negative, as the unsigned condition).
/// Against a constant the 16-bit form is cheaper (`and #$ff`, then
/// compare) than an 8-bit section, so only byte-to-byte compares narrow.
///
/// The compare then reads the byte where the widening read it before, so
/// the byte must still hold the same value there: nothing between the two
/// in one block redefines it, or it is defined once, before the widening,
/// which dominates the compare.
pub fn narrow_compares(f: &mut Func) -> bool {
    let mut uses: HashMap<VReg, u32> = HashMap::new();
    let mut count = |r: VReg| *uses.entry(r).or_default() += 1;
    for b in &f.blocks {
        for i in &b.insts {
            for u in i.uses() {
                count(u);
            }
        }
        for u in b.term.uses() {
            count(u);
        }
    }
    // Single-use widenings of a byte register: register -> (kind, byte, where).
    let mut widened: HashMap<VReg, (ConvKind, VReg, (usize, usize))> = HashMap::new();
    let defs = single_defs(f);
    for (bi, b) in f.blocks.iter().enumerate() {
        for (k, i) in b.insts.iter().enumerate() {
            if let Inst::Conv { kind: kind @ (ConvKind::Zext | ConvKind::Sext), dst, src: Operand::Reg(s), from: IrTy::I8 } = i {
                if f.ty(*dst) == IrTy::I16 && uses.get(dst) == Some(&1) && defs.contains_key(dst) {
                    widened.insert(*dst, (*kind, *s, (bi, k)));
                }
            }
        }
    }
    if widened.is_empty() {
        return false;
    }
    let idom = crate::copy::dominators(f);
    // Does `s` hold at `to` the value it held at `from`?
    let same_value = |s: VReg, from: (usize, usize), to: (usize, usize)| -> bool {
        if from.0 == to.0 && from.1 < to.1 {
            return f.blocks[from.0].insts[from.1 + 1..to.1].iter().all(|i| i.def() != Some(s));
        }
        let dominates_to = idom[to.0].is_some() && from.0 != to.0 && crate::copy::dominates(&idom, from.0, to.0);
        dominates_to && defs.get(&s).map_or(false, |&d| def_dominates(&idom, d, from))
    };
    let narrow = |cc: Cond, a: &Operand, b: &Operand, at: (usize, usize)| -> Option<(Cond, Operand, Operand)> {
        let side = |o: &Operand| match o {
            Operand::Reg(r) => widened.get(r).filter(|(_, s, from)| same_value(*s, *from, at)).map(|(k, s, _)| (*k, *s)),
            _ => None,
        };
        let (ka, na) = side(a)?;
        let (kb, nb) = side(b)?;
        if ka != kb {
            return None;
        }
        let c = match (ka, cc) {
            (ConvKind::Zext, Cond::LtS) => Cond::LtU,
            (ConvKind::Zext, Cond::LeS) => Cond::LeU,
            (ConvKind::Zext, Cond::GtS) => Cond::GtU,
            (ConvKind::Zext, Cond::GeS) => Cond::GeU,
            (ConvKind::Sext, c) if !c.is_signed() && !matches!(c, Cond::Eq | Cond::Ne) => return None,
            (_, c) => c,
        };
        Some((c, Operand::Reg(na), Operand::Reg(nb)))
    };
    let mut edits: Vec<(usize, Option<usize>, Cond, Operand, Operand)> = Vec::new();
    for (bi, b) in f.blocks.iter().enumerate() {
        for (k, i) in b.insts.iter().enumerate() {
            if let Inst::Cmp { cc, ty: IrTy::I16, a, b: bo, .. } = i {
                if let Some((c, na, nb)) = narrow(*cc, a, bo, (bi, k)) {
                    edits.push((bi, Some(k), c, na, nb));
                }
            }
        }
        if let Term::BrCmp { cc, ty: IrTy::I16, a, b: bo, .. } = &b.term {
            if let Some((c, na, nb)) = narrow(*cc, a, bo, (bi, b.insts.len())) {
                edits.push((bi, None, c, na, nb));
            }
        }
    }
    let changed = !edits.is_empty();
    for (bi, k, c, na, nb) in edits {
        match k {
            Some(k) => {
                if let Inst::Cmp { cc, ty, a, b, .. } = &mut f.blocks[bi].insts[k] {
                    (*cc, *ty, *a, *b) = (c, IrTy::I8, na, nb);
                }
            }
            None => {
                if let Term::BrCmp { cc, ty, a, b, .. } = &mut f.blocks[bi].term {
                    (*cc, *ty, *a, *b) = (c, IrTy::I8, na, nb);
                }
            }
        }
    }
    changed
}
