//! An IR interpreter over a simulated SNES address space: the reference
//! semantics for differential testing (16-bit int, far pointers whose
//! arithmetic wraps within a bank).

use crate::ir::*;
use std::collections::HashMap;

pub struct Machine<'m> {
    m: &'m Module,
    mem: Vec<u8>,
    pub symbols: HashMap<String, u32>,
    func_at: HashMap<u32, String>,
    /// Frame stack pointer (bank $7E, grows down from $FFFF).
    sp: u32,
    pub out: String,
    pub steps: u64,
    pub step_limit: u64,
    /// External functions the program may call: name -> handler.
    pub externs: HashMap<String, fn(&mut Machine, &[i64]) -> i64>,
    pub exit_requested: Option<i64>,
}

#[derive(Debug)]
pub enum Trap {
    Exit(i64),
    Error(String),
}

pub struct RunResult {
    pub exit: Result<i64, String>,
    pub out: String,
    pub steps: u64,
}

impl<'m> Machine<'m> {
    pub fn new(m: &'m Module) -> Machine<'m> {
        let mut mc = Machine {
            m,
            mem: vec![0u8; 1 << 24],
            symbols: HashMap::new(),
            func_at: HashMap::new(),
            sp: 0x7f_0000,
            out: String::new(),
            steps: 0,
            step_limit: 50_000_000,
            externs: HashMap::new(),
            exit_requested: None,
        };
        mc.install_libc();
        mc.layout();
        mc
    }

    fn layout(&mut self) {
        // RAM objects in $7E:2000.., initialised data in $7F.., ROM data in
        // $80:8000..; no object crosses a 64 KiB boundary.
        let mut bss = 0x7e_2000u32;
        let mut data = 0x7f_0000u32;
        let mut rom = 0x80_8000u32;
        for g in &self.m.globals {
            let cursor = match g.section {
                Section::Bss => &mut bss,
                Section::Data => &mut data,
                Section::Rodata => &mut rom,
            };
            let align = g.align.max(1);
            let mut a = cursor.div_ceil(align) * align;
            let size = g.size.max(1);
            if (a & 0xffff) + size > 0x10000 {
                a = (a & 0xff_0000) + 0x1_0000 + if g.section == Section::Rodata { 0x8000 } else { 0 };
            }
            self.symbols.insert(g.name.clone(), a);
            *cursor = a + size;
        }
        let mut fa = 0xc0_0000u32;
        for f in &self.m.funcs {
            self.symbols.insert(f.name.clone(), fa);
            self.func_at.insert(fa, f.name.clone());
            fa += 4;
        }
        for e in &self.m.extern_funcs {
            if !self.symbols.contains_key(&e.0) {
                self.symbols.insert(e.0.clone(), fa);
                self.func_at.insert(fa, e.0.clone());
                fa += 4;
            }
        }
        // Initial images.
        for g in &self.m.globals {
            let Some((bytes, relocs)) = &g.init else { continue };
            let base = self.symbols[&g.name];
            for (i, b) in bytes.iter().enumerate() {
                self.mem[(base as usize + i) & 0xff_ffff] = *b;
            }
            for r in relocs {
                let t = self.symbols.get(&r.target).copied().unwrap_or(0);
                let v = add_addr(t, r.addend);
                self.write(base.wrapping_add(r.offset), r.width as u32, v as i64);
            }
        }
    }

    pub fn read(&self, addr: u32, size: u32) -> i64 {
        let mut v: u64 = 0;
        for i in 0..size {
            v |= (self.mem[(addr.wrapping_add(i) & 0xff_ffff) as usize] as u64) << (8 * i);
        }
        v as i64
    }

    pub fn write(&mut self, addr: u32, size: u32, v: i64) {
        for i in 0..size {
            self.mem[(addr.wrapping_add(i) & 0xff_ffff) as usize] = ((v as u64) >> (8 * i)) as u8;
        }
    }

    pub fn read_symbol(&self, name: &str, size: u32) -> Option<i64> {
        self.symbols.get(name).map(|&a| self.read(a, size))
    }

    pub fn cstring(&self, mut addr: u32) -> String {
        let mut s = Vec::new();
        loop {
            let b = self.mem[(addr & 0xff_ffff) as usize];
            if b == 0 || s.len() > 4096 {
                break;
            }
            s.push(b);
            addr = (addr & 0xff_0000) | ((addr + 1) & 0xffff);
        }
        String::from_utf8_lossy(&s).into_owned()
    }

    /// Runs `main` (or another entry) and captures output and exit status.
    pub fn run_main(&mut self, entry: &str) -> RunResult {
        let r = self.call(entry, &[]);
        let exit = match r {
            Ok(v) => Ok(IrTy::I16.sext(v)),
            Err(Trap::Exit(c)) => Ok(c),
            Err(Trap::Error(e)) => Err(e),
        };
        RunResult { exit, out: std::mem::take(&mut self.out), steps: self.steps }
    }

    pub fn call(&mut self, name: &str, args: &[i64]) -> Result<i64, Trap> {
        let Some(f) = self.m.funcs.iter().find(|f| f.name == name) else {
            if let Some(h) = self.externs.get(name).copied() {
                return Ok(h(self, args));
            }
            return Err(Trap::Error(format!("call to undefined function '{}'", name)));
        };
        self.exec(f, args, None)
    }

    fn exec(&mut self, f: &Func, args: &[i64], sret: Option<u32>) -> Result<i64, Trap> {
        // Frame: slots laid out below sp.
        let mut slot_addr = Vec::with_capacity(f.slots.len());
        let saved_sp = self.sp;
        for s in &f.slots {
            let align = s.align.max(1);
            let mut a = self.sp.saturating_sub(s.size);
            a -= a % align;
            if (a & 0xffff) < 0x2000 {
                return Err(Trap::Error("frame stack overflow".into()));
            }
            self.sp = a;
            slot_addr.push(a);
        }
        let mut regs = vec![0i64; f.vregs.len()];
        for (i, p) in f.params.iter().enumerate() {
            let v = args.get(i).copied().unwrap_or(0);
            match p {
                ParamKind::Scalar(t) => {
                    if let Some(r) = f.param_regs[i] {
                        regs[r.0 as usize] = t.zext(v);
                    }
                }
                ParamKind::Aggregate(n) => {
                    if let Some(s) = f.param_slots[i] {
                        let dst = slot_addr[s.0 as usize];
                        for k in 0..*n {
                            let b = self.read(add_addr(v as u32, k as i64), 1);
                            self.write(dst + k, 1, b);
                        }
                    }
                }
            }
        }
        if let (Some(r), Some(a)) = (f.sret_reg, sret) {
            regs[r.0 as usize] = a as i64;
        }
        let mut b = BlockId(0);
        let result = 'outer: loop {
            let block = &f.blocks[b.0 as usize];
            for inst in &block.insts {
                self.steps += 1;
                if self.steps > self.step_limit {
                    self.sp = saved_sp;
                    return Err(Trap::Error("step limit exceeded".into()));
                }
                self.inst(f, inst, &mut regs, &slot_addr)?;
            }
            let val = |o: &Operand, regs: &Vec<i64>, me: &Self| me.operand(o, regs, &slot_addr);
            match &block.term {
                Term::Jmp(t) => b = *t,
                Term::Br { cond, t, f: fb } => {
                    let c = val(cond, &regs, self);
                    b = if c != 0 { *t } else { *fb };
                }
                Term::BrCmp { cc, ty, a, b: bo, t, f: fb } => {
                    let x = val(a, &regs, self);
                    let y = val(bo, &regs, self);
                    b = if cmp(*cc, *ty, x, y) { *t } else { *fb };
                }
                Term::Switch { val: v, ty, cases, default } => {
                    let x = ty.zext(val(v, &regs, self));
                    b = cases.iter().find(|(c, _)| ty.zext(*c) == x).map(|c| c.1).unwrap_or(*default);
                }
                Term::Ret(v) => {
                    let r = v.as_ref().map(|o| val(o, &regs, self)).unwrap_or(0);
                    break 'outer f.ret.map(|t| t.zext(r)).unwrap_or(0);
                }
                Term::Unreachable => {
                    self.sp = saved_sp;
                    return Err(Trap::Error(format!("reached unreachable in {}", f.name)));
                }
            }
        };
        self.sp = saved_sp;
        Ok(result)
    }

    fn operand(&self, o: &Operand, regs: &[i64], slots: &[u32]) -> i64 {
        match o {
            Operand::Reg(r) => regs[r.0 as usize],
            Operand::Imm(v) => *v,
            Operand::Global(g, off) => add_addr(self.symbols.get(g).copied().unwrap_or(0), *off) as i64,
            Operand::Slot(s, off) => add_addr(slots[s.0 as usize], *off) as i64,
        }
    }

    fn addr(&self, a: &Addr, regs: &[i64], slots: &[u32]) -> u32 {
        let base = match &a.base {
            Base::Global(g) => self.symbols.get(g).copied().unwrap_or(0),
            Base::Slot(s) => slots[s.0 as usize],
            Base::Reg(r) => (regs[r.0 as usize] as u32) & 0xff_ffff,
            Base::Abs(x) => *x,
        };
        let mut off = a.offset;
        if let Some((r, s)) = a.index {
            off += (regs[r.0 as usize] as i64) * s as i64;
        }
        add_addr(base, off)
    }

    fn inst(&mut self, f: &Func, i: &Inst, regs: &mut Vec<i64>, slots: &[u32]) -> Result<(), Trap> {
        let v = |o: &Operand, regs: &Vec<i64>, me: &Self| me.operand(o, regs, slots);
        match i {
            Inst::Mov { dst, src } => {
                let t = f.ty(*dst);
                regs[dst.0 as usize] = t.zext(v(src, regs, self));
            }
            Inst::Bin { op, dst, a, b } => {
                let t = f.ty(*dst);
                let x = v(a, regs, self);
                let y = v(b, regs, self);
                let r = if t == IrTy::Ptr {
                    // Pointer arithmetic: low 16 bits only.
                    match op {
                        BinOp::Add => add_addr(x as u32, IrTy::I16.sext(y)) as i64,
                        BinOp::Sub => add_addr(x as u32, -IrTy::I16.sext(y)) as i64,
                        _ => binop(*op, IrTy::I32, x, y)?,
                    }
                } else {
                    binop(*op, t, x, y).map_err(|e| match e {
                        Trap::Error(m) => Trap::Error(format!("{} (in {}: {})", m, f.name, crate::print_inst(f, i))),
                        other => other,
                    })?
                };
                regs[dst.0 as usize] = t.zext(r);
            }
            Inst::Un { op, dst, a } => {
                let t = f.ty(*dst);
                let x = v(a, regs, self);
                regs[dst.0 as usize] = t.zext(match op {
                    UnOp::Neg => x.wrapping_neg(),
                    UnOp::Not => !x,
                });
            }
            Inst::Cmp { cc, ty, dst, a, b } => {
                let x = v(a, regs, self);
                let y = v(b, regs, self);
                regs[dst.0 as usize] = cmp(*cc, *ty, x, y) as i64;
            }
            Inst::Conv { kind, dst, src, from } => {
                let t = f.ty(*dst);
                let x = v(src, regs, self);
                regs[dst.0 as usize] = match kind {
                    ConvKind::Sext => t.zext(from.sext(x)),
                    ConvKind::Zext => t.zext(from.zext(x)),
                    ConvKind::Trunc => t.zext(x),
                };
            }
            Inst::Load { dst, addr, .. } => {
                let t = f.ty(*dst);
                let a = self.addr(addr, regs, slots);
                let raw = self.read(a, t.size());
                regs[dst.0 as usize] = t.zext(raw);
            }
            Inst::Store { addr, src, ty, .. } => {
                let a = self.addr(addr, regs, slots);
                let x = v(src, regs, self);
                self.write(a, ty.size(), x);
            }
            Inst::Lea { dst, addr } => {
                regs[dst.0 as usize] = self.addr(addr, regs, slots) as i64;
            }
            Inst::Call { dst, callee, args, sret, .. } => {
                let argv: Vec<i64> = args.iter().map(|a| v(a, regs, self)).collect();
                let name = match callee {
                    Callee::Direct(n) => n.clone(),
                    Callee::Indirect(o) => {
                        let a = (v(o, regs, self) as u32) & 0xff_ffff;
                        match self.func_at.get(&a) {
                            Some(n) => n.clone(),
                            None => return Err(Trap::Error(format!("indirect call to non-function ${:06x}", a))),
                        }
                    }
                };
                let sret_addr = sret.as_ref().map(|s| self.addr(s, regs, slots));
                let r = match self.m.funcs.iter().find(|g| g.name == name) {
                    Some(g) => self.exec(g, &argv, sret_addr)?,
                    None => match self.externs.get(name.as_str()).copied() {
                        Some(h) => h(self, &argv),
                        None => {
                            if name == "exit" {
                                return Err(Trap::Exit(IrTy::I16.sext(argv.first().copied().unwrap_or(0))));
                            }
                            if name == "abort" {
                                return Err(Trap::Error("abort() called".into()));
                            }
                            return Err(Trap::Error(format!("call to undefined function '{}'", name)));
                        }
                    },
                };
                if let Some(d) = dst {
                    let t = f.ty(*d);
                    regs[d.0 as usize] = t.zext(r);
                }
                if self.exit_requested.is_some() {
                    return Err(Trap::Exit(self.exit_requested.unwrap()));
                }
            }
            Inst::Memcpy { dst, src, size } => {
                let d = self.addr(dst, regs, slots);
                let s = self.addr(src, regs, slots);
                let bytes: Vec<i64> = (0..*size).map(|k| self.read(add_addr(s, k as i64), 1)).collect();
                for (k, b) in bytes.into_iter().enumerate() {
                    self.write(add_addr(d, k as i64), 1, b);
                }
            }
            Inst::Memset { dst, val, size } => {
                let d = self.addr(dst, regs, slots);
                for k in 0..*size {
                    self.write(add_addr(d, k as i64), 1, *val as i64);
                }
            }
        }
        Ok(())
    }
}

/// bank:offset + delta, wrapping within the bank (the 65816's 16-bit index
/// arithmetic).
pub fn add_addr(base: u32, delta: i64) -> u32 {
    (base & 0xff_0000) | (((base & 0xffff) as i64 + delta) as u32 & 0xffff)
}

fn cmp(cc: Cond, ty: IrTy, x: i64, y: i64) -> bool {
    if ty == IrTy::Ptr && !matches!(cc, Cond::Eq | Cond::Ne) {
        // Relational pointer compares use the low sixteen bits.
        return cc.eval(IrTy::I16, x, y);
    }
    cc.eval(ty, x, y)
}

fn binop(op: BinOp, t: IrTy, x: i64, y: i64) -> Result<i64, Trap> {
    let (sx, sy) = (t.sext(x), t.sext(y));
    let (ux, uy) = (t.zext(x) as u64, t.zext(y) as u64);
    let bits = t.bits() as i64;
    Ok(match op {
        BinOp::Add => x.wrapping_add(y),
        BinOp::Sub => x.wrapping_sub(y),
        BinOp::Mul => x.wrapping_mul(y),
        BinOp::DivS => {
            if sy == 0 {
                return Err(Trap::Error("division by zero".into()));
            }
            sx.wrapping_div(sy)
        }
        BinOp::DivU => {
            if uy == 0 {
                return Err(Trap::Error("division by zero".into()));
            }
            (ux / uy) as i64
        }
        BinOp::RemS => {
            if sy == 0 {
                return Err(Trap::Error("division by zero".into()));
            }
            sx.wrapping_rem(sy)
        }
        BinOp::RemU => {
            if uy == 0 {
                return Err(Trap::Error("division by zero".into()));
            }
            (ux % uy) as i64
        }
        BinOp::And => x & y,
        BinOp::Or => x | y,
        BinOp::Xor => x ^ y,
        BinOp::Shl => {
            let s = t.zext(y);
            if s >= bits { 0 } else { x << s }
        }
        BinOp::ShrS => {
            let s = t.zext(y);
            if s >= bits { if sx < 0 { -1 } else { 0 } } else { sx >> s }
        }
        BinOp::ShrU => {
            let s = t.zext(y);
            if s >= bits { 0 } else { (ux >> s) as i64 }
        }
    })
}

// ---------------------------------------------------------------------- a tiny libc

impl<'m> Machine<'m> {
    fn install_libc(&mut self) {
        self.externs.insert("putchar".into(), |m, a| {
            let c = a.first().copied().unwrap_or(0) as u8;
            m.out.push(c as char);
            c as i64
        });
        self.externs.insert("puts".into(), |m, a| {
            let s = m.cstring(a.first().copied().unwrap_or(0) as u32);
            m.out.push_str(&s);
            m.out.push('\n');
            0
        });
        self.externs.insert("printf".into(), |m, a| {
            let fmt = m.cstring(a.first().copied().unwrap_or(0) as u32);
            let s = format_printf(m, &fmt, &a[1.min(a.len())..]);
            let n = s.len() as i64;
            m.out.push_str(&s);
            n
        });
        self.externs.insert("memset".into(), |m, a| {
            let (d, v, n) = (a[0] as u32, a[1], a[2] as u16 as u32);
            for k in 0..n {
                m.write(add_addr(d, k as i64), 1, v);
            }
            a[0]
        });
        self.externs.insert("memcpy".into(), |m, a| {
            let (d, s, n) = (a[0] as u32, a[1] as u32, a[2] as u16 as u32);
            for k in 0..n {
                let b = m.read(add_addr(s, k as i64), 1);
                m.write(add_addr(d, k as i64), 1, b);
            }
            a[0]
        });
        self.externs.insert("strlen".into(), |m, a| m.cstring(a[0] as u32).len() as i64);
        self.externs.insert("strcmp".into(), |m, a| {
            let (x, y) = (m.cstring(a[0] as u32), m.cstring(a[1] as u32));
            match x.cmp(&y) {
                std::cmp::Ordering::Less => -1,
                std::cmp::Ordering::Equal => 0,
                std::cmp::Ordering::Greater => 1,
            }
        });
        self.externs.insert("strcpy".into(), |m, a| {
            let (d, s) = (a[0] as u32, a[1] as u32);
            let mut k = 0i64;
            loop {
                let b = m.read(add_addr(s, k), 1);
                m.write(add_addr(d, k), 1, b);
                if b & 0xff == 0 {
                    break;
                }
                k += 1;
            }
            a[0]
        });
        self.externs.insert("strncpy".into(), |m, a| {
            let (d, s, n) = (a[0] as u32, a[1] as u32, a[2] as u16 as i64);
            let mut end = false;
            for k in 0..n {
                let b = if end { 0 } else { m.read(add_addr(s, k), 1) & 0xff };
                if b == 0 {
                    end = true;
                }
                m.write(add_addr(d, k), 1, b);
            }
            a[0]
        });
        self.externs.insert("strcat".into(), |m, a| {
            let (d, s) = (a[0] as u32, a[1] as u32);
            let mut k = m.cstring(d).len() as i64;
            let mut j = 0i64;
            loop {
                let b = m.read(add_addr(s, j), 1) & 0xff;
                m.write(add_addr(d, k), 1, b);
                if b == 0 {
                    break;
                }
                k += 1;
                j += 1;
            }
            a[0]
        });
        self.externs.insert("strncmp".into(), |m, a| {
            let (x, y, n) = (a[0] as u32, a[1] as u32, a[2] as u16 as i64);
            for k in 0..n {
                let (p, q) = (m.read(add_addr(x, k), 1) & 0xff, m.read(add_addr(y, k), 1) & 0xff);
                if p != q {
                    return if p < q { -1 } else { 1 };
                }
                if p == 0 {
                    break;
                }
            }
            0
        });
        self.externs.insert("strchr".into(), |m, a| {
            let (s, c) = (a[0] as u32, a[1] & 0xff);
            let mut k = 0i64;
            loop {
                let b = m.read(add_addr(s, k), 1) & 0xff;
                if b == c {
                    return add_addr(s, k) as i64;
                }
                if b == 0 {
                    return 0;
                }
                k += 1;
            }
        });
        self.externs.insert("memcmp".into(), |m, a| {
            let (x, y, n) = (a[0] as u32, a[1] as u32, a[2] as u16 as i64);
            for k in 0..n {
                let (p, q) = (m.read(add_addr(x, k), 1) & 0xff, m.read(add_addr(y, k), 1) & 0xff);
                if p != q {
                    return if p < q { -1 } else { 1 };
                }
            }
            0
        });
        self.externs.insert("memmove".into(), |m, a| {
            let (d, s, n) = (a[0] as u32, a[1] as u32, a[2] as u16 as u32);
            let bytes: Vec<i64> = (0..n).map(|k| m.read(add_addr(s, k as i64), 1)).collect();
            for (k, b) in bytes.into_iter().enumerate() {
                m.write(add_addr(d, k as i64), 1, b);
            }
            a[0]
        });
        self.externs.insert("abs".into(), |_, a| {
            let v = IrTy::I16.sext(a[0]);
            v.abs()
        });
        self.externs.insert("atoi".into(), |m, a| {
            let s = m.cstring(a[0] as u32);
            let t = s.trim_start();
            let (neg, digits) = match t.strip_prefix('-') {
                Some(r) => (true, r),
                None => (false, t.strip_prefix('+').unwrap_or(t)),
            };
            let n: i64 = digits.chars().take_while(|c| c.is_ascii_digit()).fold(0, |acc, c| acc * 10 + c.to_digit(10).unwrap() as i64);
            if neg { -n } else { n }
        });
        self.externs.insert("exit".into(), |m, a| {
            m.exit_requested = Some(IrTy::I16.sext(a.first().copied().unwrap_or(0)));
            0
        });
    }
}

fn format_printf(m: &Machine, fmt: &str, args: &[i64]) -> String {
    let mut out = String::new();
    let mut ai = 0;
    let mut next = || {
        let v = args.get(ai).copied().unwrap_or(0);
        ai += 1;
        v
    };
    let chars: Vec<char> = fmt.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        if chars[i] != '%' {
            out.push(chars[i]);
            i += 1;
            continue;
        }
        i += 1;
        let mut flags = String::new();
        while i < chars.len() && "-+ 0#".contains(chars[i]) {
            flags.push(chars[i]);
            i += 1;
        }
        let mut width = String::new();
        while i < chars.len() && chars[i].is_ascii_digit() {
            width.push(chars[i]);
            i += 1;
        }
        let mut long = 0;
        while i < chars.len() && (chars[i] == 'l' || chars[i] == 'h') {
            if chars[i] == 'l' {
                long += 1;
            }
            i += 1;
        }
        let Some(&c) = chars.get(i) else { break };
        i += 1;
        let w: usize = width.parse().unwrap_or(0);
        let zero = flags.contains('0');
        let left = flags.contains('-');
        let pad = |s: String| -> String {
            if s.len() >= w {
                s
            } else if left {
                format!("{:<w$}", s, w = w)
            } else if zero {
                let (sign, digits) = if s.starts_with('-') { ("-", &s[1..]) } else { ("", &s[..]) };
                format!("{}{}{}", sign, "0".repeat(w - s.len()), digits)
            } else {
                format!("{:>w$}", s, w = w)
            }
        };
        let ity = if long > 0 { IrTy::I32 } else { IrTy::I16 };
        let s = match c {
            'd' | 'i' => ity.sext(next()).to_string(),
            'u' => ity.zext(next()).to_string(),
            'x' => format!("{:x}", ity.zext(next())),
            'X' => format!("{:X}", ity.zext(next())),
            'c' => ((next() as u8) as char).to_string(),
            's' => m.cstring(next() as u32),
            'p' => format!("{:06x}", next() & 0xff_ffff),
            '%' => "%".to_string(),
            other => format!("%{}", other),
        };
        out.push_str(&pad(s));
    }
    out
}
