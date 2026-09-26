//! Per-function analyses the code generator needs: liveness, use counts,
//! which values can stay in the accumulator, and value homes.

use loomcc_ir::*;

/// A dense bit set over virtual registers.
#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Bits(Vec<u64>);

impl Bits {
    pub fn new(n: usize) -> Bits {
        Bits(vec![0; n.div_ceil(64)])
    }
    pub fn set(&mut self, i: u32) {
        self.0[(i / 64) as usize] |= 1 << (i % 64);
    }
    pub fn clear(&mut self, i: u32) {
        self.0[(i / 64) as usize] &= !(1 << (i % 64));
    }
    pub fn get(&self, i: u32) -> bool {
        self.0[(i / 64) as usize] >> (i % 64) & 1 != 0
    }
    pub fn union_with(&mut self, o: &Bits) -> bool {
        let mut changed = false;
        for (a, b) in self.0.iter_mut().zip(&o.0) {
            let n = *a | *b;
            if n != *a {
                *a = n;
                changed = true;
            }
        }
        changed
    }
    pub fn iter(&self) -> impl Iterator<Item = u32> + '_ {
        self.0.iter().enumerate().flat_map(|(w, &bits)| (0..64).filter(move |b| bits >> b & 1 != 0).map(move |b| (w * 64 + b) as u32))
    }
}

pub struct Liveness {
    pub live_in: Vec<Bits>,
    pub live_out: Vec<Bits>,
}

pub fn liveness(f: &Func) -> Liveness {
    let n = f.vregs.len();
    let nb = f.blocks.len();
    let mut gen = vec![Bits::new(n); nb];
    let mut kill = vec![Bits::new(n); nb];
    for (bi, b) in f.blocks.iter().enumerate() {
        for i in &b.insts {
            for u in i.uses() {
                if !kill[bi].get(u.0) {
                    gen[bi].set(u.0);
                }
            }
            if let Some(d) = i.def() {
                kill[bi].set(d.0);
            }
        }
        for u in b.term.uses() {
            if !kill[bi].get(u.0) {
                gen[bi].set(u.0);
            }
        }
    }
    let mut live_in = vec![Bits::new(n); nb];
    let mut live_out = vec![Bits::new(n); nb];
    let mut changed = true;
    while changed {
        changed = false;
        for bi in (0..nb).rev() {
            let mut out = Bits::new(n);
            for s in f.blocks[bi].term.succs() {
                out.union_with(&live_in[s.0 as usize]);
            }
            let mut inn = out.clone();
            for w in 0..inn.0.len() {
                inn.0[w] = gen[bi].0[w] | (out.0[w] & !kill[bi].0[w]);
            }
            if inn != live_in[bi] {
                live_in[bi] = inn;
                changed = true;
            }
            live_out[bi] = out;
        }
    }
    Liveness { live_in, live_out }
}

/// Registers live after each instruction of a block (index i: after
/// insts[i]; the last entry is before the terminator... i.e. live across
/// the terminator's uses are included).
pub fn live_after(f: &Func, lv: &Liveness, bi: usize) -> Vec<Bits> {
    let b = &f.blocks[bi];
    let mut live = lv.live_out[bi].clone();
    for u in b.term.uses() {
        live.set(u.0);
    }
    let mut out = vec![Bits::new(f.vregs.len()); b.insts.len()];
    for (k, i) in b.insts.iter().enumerate().rev() {
        out[k] = live.clone();
        if let Some(d) = i.def() {
            live.clear(d.0);
        }
        for u in i.uses() {
            live.set(u.0);
        }
    }
    out
}

pub fn use_counts(f: &Func) -> Vec<u32> {
    let mut c = vec![0u32; f.vregs.len()];
    for b in &f.blocks {
        for i in &b.insts {
            for u in i.uses() {
                c[u.0 as usize] += 1;
            }
        }
        for u in b.term.uses() {
            c[u.0 as usize] += 1;
        }
    }
    c
}

pub fn def_counts(f: &Func) -> Vec<u32> {
    let mut c = vec![0u32; f.vregs.len()];
    for b in &f.blocks {
        for i in &b.insts {
            if let Some(d) = i.def() {
                c[d.0 as usize] += 1;
            }
        }
    }
    for r in f.param_regs.iter().flatten() {
        c[r.0 as usize] += 1;
    }
    if let Some(r) = f.sret_reg {
        c[r.0 as usize] += 1;
    }
    c
}

/// Registers used as the base of a pointer access (they want a direct-page
/// home for `[dp],y`).
pub fn pointer_bases(f: &Func) -> Vec<bool> {
    let mut v = vec![false; f.vregs.len()];
    for b in &f.blocks {
        for i in &b.insts {
            let addrs: Vec<&Addr> = match i {
                Inst::Load { addr, .. } | Inst::Store { addr, .. } => vec![addr],
                Inst::Memcpy { dst, src, .. } => vec![dst, src],
                Inst::Memset { dst, .. } => vec![dst],
                Inst::Call { sret: Some(a), .. } => vec![a],
                _ => vec![],
            };
            for a in addrs {
                if let Base::Reg(r) = a.base {
                    v[r.0 as usize] = true;
                }
            }
        }
    }
    v
}

/// Loop depth of each block (for weighting), from back edges in block order.
pub fn loop_weight(f: &Func) -> Vec<u32> {
    let nb = f.blocks.len();
    let mut w = vec![1u32; nb];
    for (bi, b) in f.blocks.iter().enumerate() {
        for s in b.term.succs() {
            let s = s.0 as usize;
            if s <= bi {
                // Back edge: blocks s..=bi form (roughly) a loop body.
                for k in s..=bi {
                    w[k] = w[k].saturating_mul(8).min(1 << 20);
                }
            }
        }
    }
    w
}
