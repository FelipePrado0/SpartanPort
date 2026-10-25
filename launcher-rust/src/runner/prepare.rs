//! The SELF loader (Python `scripts/prepare.py`): turns the game's plaintext `eboot.bin` into the
//! probe's loader image (`boot.bin`, `G3PROBE2`), the unpacked `eboot.elf`, `entry.bin` and
//! `analysis.json`. No decryption; every offset read from a file is checked before use.

use std::collections::HashMap;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use indexmap::{IndexMap, IndexSet};
use serde::ser::{SerializeMap, Serializer};
use serde::Serialize;
use sha2::{Digest, Sha256};

use crate::core::fs::atomic_write;
use crate::core::sfo::{load_sfo, SfoError, SfoReadError, SfoValue};

use super::probe_image::{Extension, ImageError, ImageRelocation, ProbeImage};
use super::sce::{
    hex, hex_number, place, ImportKind, SceError, SceModule, Target, R_X86_64_64,
    R_X86_64_GLOB_DAT, R_X86_64_JUMP_SLOT, R_X86_64_RELATIVE, STT_FUNC,
};
pub use super::sce::{
    parse_self, read_self, read_self_bytes, ProgramHeader, Segment, SelfInfo, SelfSegment,
};

const RET: u8 = 0xc3;
const IOS_BASE_INIT: &str = "_ZNSt8ios_base4InitC1Ev";
const STATUS: &str = "Prepared only; execution and Vulkan are tested separately.";

/// The imports `analysis.json` names when the eboot uses them (`import_name_hints`).
const KNOWN_IMPORTS: &[&str] = &[
    "_init_env",
    "atexit",
    "exit",
    "_exit",
    "__cxa_atexit",
    "__cxa_finalize",
    "__cxa_guard_acquire",
    "__cxa_guard_release",
    "__cxa_guard_abort",
    "__stack_chk_guard",
    "__stack_chk_fail",
    "malloc",
    "free",
    "memcpy",
    "memmove",
    "memset",
    "memcmp",
    "strlen",
    "printf",
    IOS_BASE_INIT,
    "sceKernelLoadStartModule",
    "sceVideoOutOpen",
    "sceKernelCreateSema",
    "sceKernelWaitSema",
    "sceKernelPollSema",
    "sceKernelSignalSema",
    "sceKernelCancelSema",
    "sceKernelDeleteSema",
    "sceAppContentInitialize",
    "sceAppContentAppParamGetInt",
    "sceAppContentGetAddcontInfoList",
    "pthread_cond_init",
    "sceSysmoduleLoadModule",
    "pthread_mutexattr_init",
    "gettimeofday",
    "scePthreadMutexattrInit",
    "scePthreadMutexattrSettype",
    "scePthreadMutexattrDestroy",
    "scePthreadMutexInit",
    "scePthreadMutexLock",
    "scePthreadMutexTrylock",
    "scePthreadMutexUnlock",
    "scePthreadMutexDestroy",
    "scePthreadRwlockInit",
    "scePthreadRwlockDestroy",
    "scePthreadRwlockRdlock",
    "scePthreadRwlockWrlock",
    "scePthreadRwlockTryrdlock",
    "scePthreadRwlockTrywrlock",
    "scePthreadRwlockUnlock",
    "scePthreadRwlockTimedrdlock",
    "scePthreadRwlockTimedwrlock",
    "sceKernelGetDirectMemorySize",
    "sceKernelAllocateDirectMemory",
    "sceKernelMapDirectMemory",
    "sceKernelReleaseDirectMemory",
    "sceKernelMunmap",
];

/// Why a SELF could not be loaded. The texts are the Python's, except where the Python stopped on a
/// built-in error (a missing segment, tag or symbol, a bad name, a value out of range).
#[derive(Debug, thiserror::Error)]
pub enum PrepareError {
    #[error(transparent)]
    Sce(#[from] SceError),
    #[error(transparent)]
    Image(#[from] ImageError),
    #[error("relocation outside mapped segments: {0:#x}")]
    RelocationOutside(u64),
    #[error("relocation value out of range")]
    RelocationValue,
    #[error("unsupported import addend")]
    ImportAddend,
    #[error("unsupported relocation {0}")]
    UnsupportedRelocation(u32),
    #[error(transparent)]
    Sfo(#[from] SfoError),
    #[error("{}: {source}", path.display())]
    Io { path: PathBuf, source: io::Error },
}

/// `_init_env` in the game's `libc.prx`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InitEnv {
    pub symbol: String,
    pub address: u64,
    pub size: u64,
    /// Its code: the port relies on it being one `RET`.
    pub bytes: Vec<u8>,
}

/// `std::ios_base::Init::Init()` in the game's `libc.prx`.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct IosBaseInit {
    pub symbol: String,
    #[serde(serialize_with = "hex_number")]
    pub address: u64,
    pub size: u64,
    pub code_sha256: String,
    pub name: &'static str,
}

/// What `inspect_libc` proved (`analysis.json`'s `libc_evidence`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LibcReport {
    /// SHA-256 of the file, hex.
    pub sha256: String,
    /// `_init_env` exists and its code is exactly `RET`.
    pub init_env_is_ret: bool,
    pub init_env: Option<InitEnv>,
    pub ios_base_init: Option<IosBaseInit>,
    /// The Python's dict keeps the order the symbols came in: `ios_base_init` before the keys of
    /// `_init_env` when its symbol came first.
    ios_base_init_first: bool,
}

impl Serialize for LibcReport {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        let ios_base_init = self.ios_base_init.as_ref();
        let (before, after) = if self.ios_base_init_first {
            (ios_base_init, None)
        } else {
            (None, ios_base_init)
        };
        let mut map = serializer.serialize_map(None)?;
        map.serialize_entry("sha256", &self.sha256)?;
        map.serialize_entry("init_env_is_ret", &self.init_env_is_ret)?;
        if let Some(ios) = before {
            map.serialize_entry("ios_base_init", ios)?;
        }
        if let Some(init) = &self.init_env {
            map.serialize_entry("symbol", &init.symbol)?;
            map.serialize_entry("address", &format!("{:#x}", init.address))?;
            map.serialize_entry("size", &init.size)?;
            map.serialize_entry("bytes", &hex(&init.bytes))?;
        }
        if let Some(ios) = after {
            map.serialize_entry("ios_base_init", ios)?;
        }
        map.end()
    }
}

/// What `prepare` found: `analysis.json`, in the Python's key order.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct Report {
    pub source_sha256: String,
    pub source_bytes: u64,
    #[serde(serialize_with = "sfo_values")]
    pub sfo: IndexMap<String, SfoValue>,
    #[serde(serialize_with = "hex_number")]
    pub entry: u64,
    pub image_bytes: u64,
    pub program_headers: Vec<ProgramHeader>,
    pub self_segments: Vec<SelfSegment>,
    pub unavailable_metadata_headers: Vec<usize>,
    pub needed: Vec<String>,
    /// Relocations by type, in the order the types first appear.
    pub relocation_counts: IndexMap<u32, u64>,
    pub import_count: usize,
    pub imports: Vec<String>,
    pub import_name_hints: IndexMap<String, &'static str>,
    pub libc_evidence: LibcReport,
    pub bundled_modules: Vec<String>,
    /// Files under `dvdroot_ps4` by their first path component.
    pub resources: IndexMap<String, u64>,
    pub resource_bytes: u64,
    pub status: &'static str,
}

fn io_error(path: &Path) -> impl FnOnce(io::Error) -> PrepareError + '_ {
    move |source| PrepareError::Io {
        path: path.to_path_buf(),
        source,
    }
}

fn sfo_values<S: Serializer>(
    values: &IndexMap<String, SfoValue>,
    serializer: S,
) -> Result<S::Ok, S::Error> {
    let mut map = serializer.serialize_map(Some(values.len()))?;
    for (key, value) in values {
        match value {
            SfoValue::Int(number) => map.serialize_entry(key, number)?,
            SfoValue::Text(text) => map.serialize_entry(key, text)?,
        }
    }
    map.end()
}

/// The NID of a symbol name: the first 8 bytes of its salted SHA-1, reversed, in 11 characters of
/// base64 whose `/` is `-`.
pub fn nid(name: &str) -> String {
    const SALT: [u8; 16] = [
        0x51, 0x8d, 0x64, 0xa6, 0x35, 0xde, 0xd8, 0xc1, 0xe6, 0xb0, 0x39, 0xb1, 0xc3, 0xe5, 0x52,
        0x30,
    ];
    let mut input = name.as_bytes().to_vec();
    input.extend_from_slice(&SALT);
    let head: Vec<u8> = sha1(&input).iter().take(8).rev().copied().collect();
    base64_unpadded(&head).replace('/', "-")
}

/// The part of a symbol name before its first `#`: the NID without the library and module ids.
fn nid_part(name: &str) -> &str {
    name.split_once('#').map_or(name, |(head, _)| head)
}

fn base64_unpadded(bytes: &[u8]) -> String {
    const ALPHABET: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut text = String::new();
    for chunk in bytes.chunks(3) {
        let byte = |i: usize| u32::from(chunk.get(i).copied().unwrap_or(0));
        let group = byte(0) << 16 | byte(1) << 8 | byte(2);
        for i in 0..=chunk.len() {
            let digit = (group >> (18 - 6 * i)) & 63;
            text.push(char::from(ALPHABET[digit as usize]));
        }
    }
    text
}

/// SHA-1 (FIPS 180-4), for the NIDs only.
fn sha1(data: &[u8]) -> [u8; 20] {
    let mut state: [u32; 5] = [
        0x6745_2301,
        0xefcd_ab89,
        0x98ba_dcfe,
        0x1032_5476,
        0xc3d2_e1f0,
    ];
    let mut message = data.to_vec();
    message.push(0x80);
    while message.len() % 64 != 56 {
        message.push(0);
    }
    message.extend_from_slice(&(data.len() as u64).wrapping_mul(8).to_be_bytes());
    for block in message.chunks_exact(64) {
        let mut w = [0u32; 80];
        for (word, bytes) in w.iter_mut().zip(block.chunks_exact(4)) {
            *word = u32::from_be_bytes(bytes.try_into().unwrap_or_default());
        }
        for i in 16..80 {
            w[i] = (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]).rotate_left(1);
        }
        let [mut a, mut b, mut c, mut d, mut e] = state;
        for (i, word) in w.into_iter().enumerate() {
            let (f, k) = match i {
                0..=19 => ((b & c) | (!b & d), 0x5a82_7999),
                20..=39 => (b ^ c ^ d, 0x6ed9_eba1),
                40..=59 => ((b & c) | (b & d) | (c & d), 0x8f1b_bcdc),
                _ => (b ^ c ^ d, 0xca62_c1d6),
            };
            let next = a
                .rotate_left(5)
                .wrapping_add(f)
                .wrapping_add(e)
                .wrapping_add(k)
                .wrapping_add(word);
            e = d;
            d = c;
            c = b.rotate_left(30);
            b = a;
            a = next;
        }
        for (h, value) in state.iter_mut().zip([a, b, c, d, e]) {
            *h = h.wrapping_add(value);
        }
    }
    let mut digest = [0; 20];
    for (bytes, h) in digest.chunks_exact_mut(4).zip(state) {
        bytes.copy_from_slice(&h.to_be_bytes());
    }
    digest
}

/// Proves that this dump's `_init_env` is exactly `RET`; never assumes it.
pub fn inspect_libc(path: &Path) -> Result<LibcReport, PrepareError> {
    let module = SceModule::read(path)?;
    let mut report = LibcReport {
        sha256: hex(&module.sha256),
        init_env_is_ret: false,
        init_env: None,
        ios_base_init: None,
        ios_base_init_first: false,
    };
    let init_env = nid("_init_env");
    let ios_base_init = nid(IOS_BASE_INIT);
    for symbol in &module.symbols {
        let key = nid_part(&symbol.name);
        if (key != init_env && key != ios_base_init)
            || symbol.section == 0
            || symbol.kind != STT_FUNC
        {
            continue;
        }
        let code = module.code(symbol)?;
        if key == init_env {
            report.init_env_is_ret = code == [RET];
            report.init_env = Some(InitEnv {
                symbol: symbol.name.clone(),
                address: symbol.value,
                size: symbol.size,
                bytes: code.to_vec(),
            });
        } else {
            if report.ios_base_init.is_none() && report.init_env.is_none() {
                report.ios_base_init_first = true;
            }
            report.ios_base_init = Some(IosBaseInit {
                symbol: symbol.name.clone(),
                address: symbol.value,
                size: symbol.size,
                code_sha256: hex(&Sha256::digest(code)),
                name: IOS_BASE_INIT,
            });
        }
    }
    Ok(report)
}

/// Adds the eboot's relocations (PLT, then RELA) to `boot` and its imports' names, in the order of
/// their numbers, and returns how many there are of each type, in the order the types first
/// appear. Absolute symbols are written into the image here.
fn relocate(module: &SceModule, boot: &mut ProbeImage) -> Result<IndexMap<u32, u64>, PrepareError> {
    let mut counts = IndexMap::new();
    let mut imports = IndexSet::new();
    for r in module.relocations()? {
        *counts.entry(r.kind).or_insert(0) += 1;
        if !module.mapped(r.target, 8) {
            return Err(PrepareError::RelocationOutside(r.target));
        }
        match r.kind {
            R_X86_64_RELATIVE => boot.relocate(r.target, i128::from(r.addend)),
            R_X86_64_64 | R_X86_64_GLOB_DAT | R_X86_64_JUMP_SLOT => match module.resolve(&r)? {
                Target::Absolute(sum) => {
                    let value = u64::try_from(sum).map_err(|_| PrepareError::RelocationValue)?;
                    place(&mut boot.image, r.target, &value.to_le_bytes())?;
                }
                Target::Defined(sum) => {
                    let value = i64::try_from(sum).map_err(|_| PrepareError::RelocationValue)?;
                    boot.relocate(r.target, i128::from(value));
                }
                Target::Undefined(symbol) => {
                    let kind = ImportKind::of(symbol);
                    if !kind.allows(r.addend) {
                        return Err(PrepareError::ImportAddend);
                    }
                    let (import, new) = imports.insert_full(r.symbol);
                    if new {
                        boot.names.push(symbol.name.clone());
                    }
                    boot.relocations.push(ImageRelocation {
                        target: r.target,
                        kind: kind as u64,
                        value: import as i128,
                        addend: r.addend,
                    });
                }
            },
            _ => return Err(PrepareError::UnsupportedRelocation(r.kind)),
        }
    }
    Ok(counts)
}

/// The files under `root` by their first path component, and their total size. The files are
/// those `Path.rglob('*')` yields, in the order Python 3.14 yields them, which sets the key order:
/// the entries of `root`, then, popping folders off a stack, the entries of each folder's
/// subfolders. A link to a folder is not followed.
fn resources(root: &Path) -> (IndexMap<String, u64>, u64) {
    let mut found = listing(root);
    let mut stack = vec![root.to_path_buf()];
    while let Some(folder) = stack.pop() {
        for (path, is_dir) in listing(&folder) {
            if is_dir {
                found.extend(listing(&path));
                stack.push(path);
            }
        }
    }
    let mut counts = IndexMap::new();
    let mut total: u64 = 0;
    for (path, _) in found {
        let Ok(metadata) = fs::metadata(&path) else {
            continue;
        };
        if !metadata.is_file() {
            continue;
        }
        let Some(top) = path
            .strip_prefix(root)
            .ok()
            .and_then(|relative| relative.components().next())
        else {
            continue;
        };
        *counts
            .entry(top.as_os_str().to_string_lossy().into_owned())
            .or_insert(0) += 1;
        total = total.saturating_add(metadata.len());
    }
    (counts, total)
}

/// The entries of `folder`, each with whether it is a folder (not following links); none when the
/// folder cannot be listed.
fn listing(folder: &Path) -> Vec<(PathBuf, bool)> {
    fs::read_dir(folder)
        .and_then(|entries| {
            entries
                .map(|entry| {
                    let entry = entry?;
                    let is_dir = entry.file_type().is_ok_and(|kind| kind.is_dir());
                    Ok((entry.path(), is_dir))
                })
                .collect()
        })
        .unwrap_or_default()
}

fn write(path: &Path, bytes: &[u8]) -> Result<(), PrepareError> {
    atomic_write(path, bytes).map_err(io_error(path))
}

/// Writes `report` as UTF-8 JSON the way the Python's `json.dumps(indent=2, ensure_ascii=False)`
/// does, plus a newline.
fn write_report(path: &Path, report: &impl Serialize) -> Result<(), PrepareError> {
    let mut text = serde_json::to_string_pretty(report)
        .map_err(|error| io_error(path)(io::Error::other(error)))?;
    text.push('\n');
    write(path, text.as_bytes())
}

/// Turns `<game>/eboot.bin` into `boot.bin`, `eboot.elf`, `entry.bin` and `analysis.json` in `out`.
pub fn prepare(game: &Path, out: &Path) -> Result<Report, PrepareError> {
    let module = SceModule::read(&game.join("eboot.bin"))?;
    // `boot.bin`, the small format `probe.c` reads.
    let mut boot = ProbeImage {
        entry: module.info.entry,
        flags: 0,
        segments: module
            .info
            .segments
            .iter()
            .map(|p| [p.vaddr, p.memsz, u64::from(p.flags)])
            .collect(),
        names: Vec::new(),
        relocations: Vec::new(),
        image: module.memory_image()?,
        extension: Extension::None,
    };
    let relocation_counts = relocate(&module, &mut boot)?;
    let known: HashMap<String, &'static str> = KNOWN_IMPORTS
        .iter()
        .map(|&name| (nid(name), name))
        .collect();
    let mut hints = IndexMap::new();
    for name in &boot.names {
        if let Some(&hint) = known.get(nid_part(name)) {
            hints.insert(name.clone(), hint);
        }
    }
    let libc = inspect_libc(&game.join("sce_module").join("libc.prx"))?;
    boot.flags = u64::from(libc.init_env_is_ret);
    write(&out.join("boot.bin"), &boot.encode()?)?;
    write(&out.join("eboot.elf"), &module.info.elf)?;
    let image = &boot.image;
    write(&out.join("entry.bin"), image.get(..1024).unwrap_or(image))?;
    let (resources, resource_bytes) = resources(&game.join("dvdroot_ps4"));
    let sfo_path = game.join("sce_sys").join("param.sfo");
    let sfo = load_sfo(&sfo_path).map_err(|error| match error {
        SfoReadError::Io(source) => io_error(&sfo_path)(source),
        SfoReadError::Invalid(error) => PrepareError::Sfo(error),
    })?;
    let needed = module.needed()?;
    let modules = game.join("sce_module");
    let mut bundled_modules = fs::read_dir(&modules)
        .and_then(|entries| {
            entries
                .map(|entry| Ok(entry?.file_name().to_string_lossy().into_owned()))
                .collect::<io::Result<Vec<_>>>()
        })
        .map_err(io_error(&modules))?;
    bundled_modules.sort();
    let report = Report {
        source_sha256: hex(&module.sha256),
        source_bytes: module.bytes,
        sfo,
        entry: module.info.entry,
        image_bytes: boot.image.len() as u64,
        program_headers: module.info.program_headers,
        self_segments: module.info.self_segments,
        unavailable_metadata_headers: module.info.missing,
        needed,
        relocation_counts,
        import_count: boot.names.len(),
        imports: boot.names,
        import_name_hints: hints,
        libc_evidence: libc,
        bundled_modules,
        resources,
        resource_bytes,
        status: STATUS,
    };
    write_report(&out.join("analysis.json"), &report)?;
    Ok(report)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::sfo::{load_sfo, parse_sfo, read_sfo, SfoReadError};
    use crate::runner::sce::tests::{
        assert_fails, elf_header, pack, program_header, put, self_entry,
    };

    /// The libc of `test_libc_ret_contract_is_verified_from_symbol_and_code`: `_init_env` at
    /// address 0 is `instruction`, found through the dynamic tags and the symbol table.
    fn synthetic_libc(instruction: u8) -> Vec<u8> {
        let mut data = vec![0; 0x320];
        put(&mut data, 0, b"O\x15=\x1d");
        put(&mut data, 24, &pack(&[(2, 2)]));
        put(&mut data, 32, &self_entry([0x800, 0x200, 16, 16]));
        put(&mut data, 64, &self_entry([0x100800, 0x220, 256, 256]));
        put(&mut data, 96, &elf_header(0xfe18, 3));
        put(
            &mut data,
            160,
            &program_header([1, 5, 0x1000, 0, 0, 16, 16, 0x1000]),
        );
        put(
            &mut data,
            216,
            &program_header([0x6100_0000, 4, 0x2000, 0, 0, 256, 0, 16]),
        );
        put(
            &mut data,
            272,
            &program_header([2, 4, 0x20b0, 0, 0, 80, 80, 8]),
        );
        data[0x200] = instruction;
        put(&mut data, 0x220, b"bzQExy189ZI#C#A\0");
        put(
            &mut data,
            0x260,
            &pack(&[(0, 4), (0x12, 1), (0, 1), (1, 2), (0, 8), (1, 8)]),
        );
        let tags = [
            (0x6100_0035, 0),
            (0x6100_0037, 32),
            (0x6100_0039, 64),
            (0x6100_003f, 24),
            (0, 0),
        ];
        for (i, (tag, value)) in tags.into_iter().enumerate() {
            put(&mut data, 0x2d0 + 16 * i, &pack(&[(tag, 8), (value, 8)]));
        }
        data
    }

    #[test]
    fn libc_ret_contract_is_verified_from_symbol_and_code() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("libc.prx");
        for unpacked in [false, true] {
            let write = |instruction| {
                let source = synthetic_libc(instruction);
                let bytes = if unpacked {
                    parse_self(&source).unwrap().elf
                } else {
                    source
                };
                std::fs::write(&path, bytes).unwrap();
            };
            write(0xc3);
            let proof = inspect_libc(&path).unwrap();
            assert!(proof.init_env_is_ret, "unpacked={unpacked}");
            assert_eq!(proof.init_env.unwrap().bytes, [0xc3]);
            write(0x90);
            assert!(
                !inspect_libc(&path).unwrap().init_env_is_ret,
                "unpacked={unpacked}"
            );
        }
        assert_eq!(nid("_init_env"), "bzQExy189ZI");
    }

    #[test]
    fn inspect_libc_stops_at_dt_null() {
        // A tag after DT_NULL (here an empty symbol table) is not part of the dynamic section.
        let mut elf = parse_self(&synthetic_libc(0xc3)).unwrap().elf;
        let dynamic = 0x20b0;
        elf.resize(dynamic + 96, 0);
        put(&mut elf, dynamic + 80, &pack(&[(0x6100_003f, 8), (0, 8)]));
        // The dynamic header is the third; its filesz and memsz follow offset, vaddr and paddr.
        put(&mut elf, 64 + 2 * 56 + 32, &pack(&[(96, 8), (96, 8)]));
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("libc.prx");
        std::fs::write(&path, elf).unwrap();
        let proof = inspect_libc(&path).unwrap();
        assert!(proof.init_env_is_ret);
        assert_eq!(proof.init_env.unwrap().bytes, [0xc3]);
    }

    #[test]
    fn report_with_non_latin_title_is_written_as_utf8() {
        let title = "ゴッド・オブ・ウォーIII リマスタード";
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("analysis.json");
        write_report(&path, &serde_json::json!({"sfo": {"TITLE": title}})).unwrap();
        let text = String::from_utf8(std::fs::read(&path).unwrap()).unwrap();
        assert!(text.contains(title), "{text}");
    }

    #[test]
    fn empty_game_files_are_named() {
        // An empty eboot.bin (a broken copy, or a placeholder made by hand) failed as
        // "out of bounds: 0x0+0x4 / 0x0", which names neither the file nor the cause.
        let dir = tempfile::tempdir().unwrap();
        let game = dir.path();
        std::fs::write(game.join("eboot.bin"), b"").unwrap();
        assert_fails(prepare(game, &game.join("out")), "eboot.bin is empty");
        // Every SELF the port loads goes through read_self, so a prx is named the same way.
        std::fs::write(game.join("libc.prx"), b"").unwrap();
        assert_fails(inspect_libc(&game.join("libc.prx")), "libc.prx is empty");
        assert_fails(read_self(&game.join("libc.prx")), "libc.prx is empty");
        // load_sfo tells a file that cannot be read from one that is not a param.sfo; read_sfo
        // answers None for both instead of failing.
        std::fs::write(game.join("param.sfo"), b"x").unwrap();
        assert!(matches!(
            load_sfo(&game.join("param.sfo")),
            Err(SfoReadError::Invalid(_))
        ));
        assert!(matches!(
            load_sfo(&game.join("none.sfo")),
            Err(SfoReadError::Io(_))
        ));
        assert_eq!(read_sfo(&game.join("param.sfo")), None);
        assert_eq!(read_sfo(&game.join("none.sfo")), None);
        assert_eq!(read_sfo(game), None);
        let message = parse_sfo(b"").unwrap_err().to_string();
        assert!(message.contains("param.sfo is empty"), "{message}");
        assert_fails(parse_self(b""), "empty SELF");
    }

    #[test]
    fn sha1_matches_the_standard_vectors() {
        // `nid` hashes with a SHA-1 of its own; these are the FIPS 180 examples, the last one
        // padded into a second block.
        assert_eq!(hex(&sha1(b"")), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
        assert_eq!(
            hex(&sha1(b"abc")),
            "a9993e364706816aba3e25717850c26c9cd0d89d"
        );
        assert_eq!(
            hex(&sha1(
                b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
            )),
            "84983e441c3bd26ebaae4aa1f95129e5e54670f1"
        );
    }
}
