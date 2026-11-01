//! The SCE container layer: a plaintext SELF or an unpacked ELF, parsed into the ELF image, its
//! program headers and the SELF's segment table, and read as a module (dynamic tags, ids, symbols
//! and relocations). No decryption; every offset read from a file is checked before use.

use std::collections::HashMap;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use indexmap::IndexMap;
use serde::ser::Serializer;
use serde::Serialize;
use sha2::{Digest, Sha256};

/// Neither the unpacked ELF nor the memory image may be larger.
pub(super) const IMAGE_LIMIT: u64 = 512 * 1024 * 1024;

pub(super) const PT_LOAD: u32 = 1;
/// `PF_X`: a segment the CPU may execute.
pub(super) const PF_X: u32 = 1;
pub(super) const PT_DYNAMIC: u32 = 2;
pub(super) const PT_TLS: u32 = 7;
pub(super) const PT_SCE_DYNLIBDATA: u32 = 0x6100_0000;
pub(super) const PT_SCE_PROCPARAM: u32 = 0x6100_0001;
const PT_SCE_RELRO: u32 = 0x6100_0010;

pub(super) const DT_NULL: u64 = 0;
const DT_NEEDED: u64 = 1;
/// `DT_INIT`: the module's initializer, relative to its base.
pub(super) const DT_INIT: u64 = 12;
pub(super) const DT_SCE_JMPREL: u64 = 0x6100_0029;
pub(super) const DT_SCE_PLTRELSZ: u64 = 0x6100_002d;
pub(super) const DT_SCE_RELA: u64 = 0x6100_002f;
pub(super) const DT_SCE_RELASZ: u64 = 0x6100_0031;
pub(super) const DT_SCE_STRTAB: u64 = 0x6100_0035;
pub(super) const DT_SCE_STRSZ: u64 = 0x6100_0037;
pub(super) const DT_SCE_SYMTAB: u64 = 0x6100_0039;
pub(super) const DT_SCE_SYMTABSZ: u64 = 0x6100_003f;
/// The dynamic tags of a library id (`DT_SCE_EXPORT_LIB`, `DT_SCE_IMPORT_LIB`) and of a module id
/// (`DT_SCE_MODULE_INFO`, `DT_SCE_NEEDED_MODULE`): the id in the top 16 bits, then the version,
/// then the name's offset in the string table.
const LIBRARY_TAGS: [u64; 2] = [0x6100_0013, 0x6100_0015];
const MODULE_TAGS: [u64; 2] = [0x6100_000d, 0x6100_000f];

pub(super) const SHN_ABS: u16 = 0xfff1;
pub(super) const STT_OBJECT: u8 = 1;
pub(super) const STT_FUNC: u8 = 2;
pub(super) const STT_SECTION: u8 = 3;
pub(super) const STB_GLOBAL: u8 = 1;
pub(super) const STB_WEAK: u8 = 2;

pub(super) const R_X86_64_64: u32 = 1;
pub(super) const R_X86_64_GLOB_DAT: u32 = 6;
pub(super) const R_X86_64_JUMP_SLOT: u32 = 7;
pub(super) const R_X86_64_RELATIVE: u32 = 8;
pub(super) const R_X86_64_DTPMOD64: u32 = 16;

/// `Elf64_Sym` and `Elf64_Rela` are both 24 bytes.
pub(super) const SYMBOL_SIZE: u64 = 24;
pub(super) const RELOCATION_SIZE: u64 = 24;

const SELF_MAGIC: &[u8] = b"O\x15=\x1d";
const ELF_MAGIC: &[u8] = b"\x7fELF";
/// `e_ident[0..7]` of a 64-bit, little-endian, version 1 ELF.
const ELF_IDENT: &[u8] = b"\x7fELF\x02\x01\x01";
const EM_X86_64: u16 = 62;

/// SELF segment flags: the entry carries a program header's bytes (its index in bits 20..32)…
const SELF_HOLDS_SEGMENT: u64 = 0x800;
/// …which are encrypted (2) or compressed (8).
const SELF_ENCRYPTED_OR_COMPRESSED: u64 = 0x2 | 0x8;

/// Why a module could not be read. The texts are the Python's, except where the Python stopped on a
/// built-in error (a missing segment or tag, a bad name or index).
#[derive(Debug, thiserror::Error)]
pub enum SceError {
    #[error("empty SELF (0 bytes)")]
    EmptySelf,
    #[error("{} is empty (0 bytes): the game files are incomplete", file_name(.0))]
    EmptyFile(PathBuf),
    #[error("out of bounds: {offset:#x}+{size:#x} / {len:#x}")]
    Truncated { offset: u64, size: u64, len: u64 },
    #[error("expected PS4 SELF or ELF")]
    NotSelf,
    #[error("expected little-endian x86-64 ELF")]
    NotX86_64,
    #[error("expected little-endian x86-64 ELF")]
    BigEndian,
    #[error("unsupported program headers")]
    BadProgramHeader,
    #[error("probe image exceeds 512 MiB limit")]
    ImageTooLarge,
    #[error("encrypted/compressed SELF segment is unsupported")]
    Encrypted,
    #[error("invalid segment index")]
    BadSegmentIndex,
    #[error("unsupported blocked segment layout")]
    BlockedSegment,
    #[error("required segment {0} is unavailable")]
    RequiredSegment(usize),
    /// A program header the loader needs is absent (the Python stops with an empty message).
    #[error("no {0} segment")]
    MissingSegment(&'static str),
    #[error("no dynamic tag {0:#x}")]
    MissingTag(u64),
    #[error("invalid memory image")]
    InvalidImage,
    #[error("segment file size exceeds memory size")]
    SegmentTooLarge,
    #[error("bad name at {0:#x} of the string table")]
    BadName(u64),
    #[error("bad symbol table size")]
    BadSymbolTable,
    #[error("bad relocation table size")]
    BadRelocationTable,
    #[error("relocation symbol {0} does not exist")]
    BadSymbolIndex(u64),
    #[error("conflicting module/library IDs")]
    ConflictingIds,
    #[error("unsupported symbol encoding '{0}'")]
    SymbolEncoding(String),
    #[error("no library or module with id '{0}'")]
    UnknownId(String),
    #[error("{}: {source}", path.display())]
    Io { path: PathBuf, source: io::Error },
}

fn file_name(path: &Path) -> String {
    path.file_name()
        .map(|name| name.to_string_lossy().into_owned())
        .unwrap_or_default()
}

fn io_error(path: &Path) -> impl FnOnce(io::Error) -> SceError + '_ {
    move |source| SceError::Io {
        path: path.to_path_buf(),
        source,
    }
}

pub(super) fn hex_number<S: Serializer>(value: &u64, serializer: S) -> Result<S::Ok, S::Error> {
    serializer.serialize_str(&format!("{value:#x}"))
}

/// `bytes` in lowercase hex, as the reports write a SHA-256.
pub(super) fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

fn out_of_bounds(data: &[u8], offset: u64, size: u64) -> SceError {
    SceError::Truncated {
        offset,
        size,
        len: data.len() as u64,
    }
}

/// `data[offset..offset + size]`, or the out-of-bounds error.
pub(super) fn span(data: &[u8], offset: u64, size: u64) -> Result<&[u8], SceError> {
    let range = usize::try_from(offset).ok().zip(
        offset
            .checked_add(size)
            .and_then(|end| usize::try_from(end).ok()),
    );
    range
        .and_then(|(start, end)| data.get(start..end))
        .ok_or_else(|| out_of_bounds(data, offset, size))
}

fn array<const N: usize>(data: &[u8], offset: u64) -> Result<[u8; N], SceError> {
    span(data, offset, N as u64)?
        .try_into()
        .map_err(|_| out_of_bounds(data, offset, N as u64))
}

pub(super) fn u8_at(data: &[u8], offset: u64) -> Result<u8, SceError> {
    Ok(u8::from_le_bytes(array(data, offset)?))
}

pub(super) fn u16_at(data: &[u8], offset: u64) -> Result<u16, SceError> {
    Ok(u16::from_le_bytes(array(data, offset)?))
}

pub(super) fn u32_at(data: &[u8], offset: u64) -> Result<u32, SceError> {
    Ok(u32::from_le_bytes(array(data, offset)?))
}

pub(super) fn u64_at(data: &[u8], offset: u64) -> Result<u64, SceError> {
    Ok(u64::from_le_bytes(array(data, offset)?))
}

pub(super) fn i64_at(data: &[u8], offset: u64) -> Result<i64, SceError> {
    Ok(i64::from_le_bytes(array(data, offset)?))
}

/// Copies `bytes` to offset `at` of `target`, or the out-of-bounds error when they do not fit.
pub(super) fn place(target: &mut [u8], at: u64, bytes: &[u8]) -> Result<(), SceError> {
    let error = out_of_bounds(target, at, bytes.len() as u64);
    let start = usize::try_from(at).ok();
    let end = start.and_then(|start| start.checked_add(bytes.len()));
    let slot = start
        .zip(end)
        .and_then(|(start, end)| target.get_mut(start..end))
        .ok_or(error)?;
    slot.copy_from_slice(bytes);
    Ok(())
}

fn tag_of(tags: &HashMap<u64, u64>, key: u64) -> Result<u64, SceError> {
    tags.get(&key).copied().ok_or(SceError::MissingTag(key))
}

/// The table of the library blob that the dynamic tags `offset_tag` and `size_tag` locate.
pub(super) fn table<'a>(
    blob: &'a [u8],
    tags: &HashMap<u64, u64>,
    offset_tag: u64,
    size_tag: u64,
) -> Result<&'a [u8], SceError> {
    let offset = tag_of(tags, offset_tag)?;
    span(blob, offset, tag_of(tags, size_tag)?)
}

/// The NUL-terminated ASCII name at `offset` of the string table.
pub(super) fn name_at(strings: &[u8], offset: u64) -> Result<String, SceError> {
    span(strings, offset, 1)?;
    let bad = || SceError::BadName(offset);
    let tail = usize::try_from(offset)
        .ok()
        .and_then(|start| strings.get(start..))
        .ok_or_else(bad)?;
    let len = tail.iter().position(|&b| b == 0).ok_or_else(bad)?;
    let name = tail
        .get(..len)
        .filter(|name| name.is_ascii())
        .ok_or_else(bad)?;
    String::from_utf8(name.to_vec()).map_err(|_| bad())
}

/// Whether `size` bytes at `address` lie inside one loadable segment.
pub(super) fn mapped(loads: &[Segment], address: u64, size: u64) -> bool {
    loads.iter().any(|p| {
        p.vaddr <= address
            && u128::from(address) + u128::from(size) <= u128::from(p.vaddr) + u128::from(p.memsz)
    })
}

/// A segment as the loaders map it: where its bytes sit in the ELF and where they go in memory.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Segment {
    pub offset: u64,
    pub vaddr: u64,
    pub filesz: u64,
    pub memsz: u64,
    pub flags: u32,
}

/// One ELF program header (`<IIQQQQQQ`), serialized in `analysis.json`'s key order.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ProgramHeader {
    #[serde(rename = "type")]
    pub kind: u32,
    pub flags: u32,
    pub offset: u64,
    pub vaddr: u64,
    pub paddr: u64,
    pub filesz: u64,
    pub memsz: u64,
    pub align: u64,
}

impl ProgramHeader {
    fn segment(&self) -> Segment {
        Segment {
            offset: self.offset,
            vaddr: self.vaddr,
            filesz: self.filesz,
            memsz: self.memsz,
            flags: self.flags,
        }
    }
}

/// One entry of a SELF's segment table.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct SelfSegment {
    #[serde(serialize_with = "hex_number")]
    pub flags: u64,
    pub offset: u64,
    pub size: u64,
}

/// A parsed SELF or unpacked ELF.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SelfInfo {
    /// The ELF with its headers and every segment at its program header's file offset.
    pub elf: Vec<u8>,
    /// `e_entry`.
    pub entry: u64,
    /// The loadable segments (`PT_LOAD`, `PT_SCE_RELRO`) in header order: what the probe maps.
    pub segments: Vec<Segment>,
    pub program_headers: Vec<ProgramHeader>,
    /// The SELF's segment table; empty for an unpacked ELF.
    pub self_segments: Vec<SelfSegment>,
    /// The program headers whose bytes the SELF does not carry (non-loadable metadata only).
    pub missing: Vec<usize>,
}

fn read_program_header(data: &[u8], offset: u64) -> Result<ProgramHeader, SceError> {
    let record = span(data, offset, 56)?;
    Ok(ProgramHeader {
        kind: u32_at(record, 0)?,
        flags: u32_at(record, 4)?,
        offset: u64_at(record, 8)?,
        vaddr: u64_at(record, 16)?,
        paddr: u64_at(record, 24)?,
        filesz: u64_at(record, 32)?,
        memsz: u64_at(record, 40)?,
        align: u64_at(record, 48)?,
    })
}

/// Parses a plaintext SELF or an unpacked ELF (x86-64, little-endian) and lays its segments out at
/// their program headers' file offsets. Encrypted or compressed segments are refused.
pub fn parse_self(data: &[u8]) -> Result<SelfInfo, SceError> {
    if data.is_empty() {
        // read_self names the file; this covers every other way in.
        return Err(SceError::EmptySelf);
    }
    let magic = span(data, 0, 4)?;
    let plain_elf = magic == ELF_MAGIC;
    let (count, base) = if plain_elf {
        (0, 0)
    } else if magic == SELF_MAGIC {
        let count = u64::from(u16_at(data, 24)?);
        (count, 32 + count * 32)
    } else {
        return Err(SceError::NotSelf);
    };
    let header = span(data, base, 64)?;
    let ident = span(header, 0, 7)?;
    if ident.get(5) != Some(&1) {
        return Err(SceError::BigEndian);
    }
    if ident != ELF_IDENT || u16_at(header, 18)? != EM_X86_64 {
        return Err(SceError::NotX86_64);
    }
    let entry = u64_at(header, 24)?;
    let phoff = u64_at(header, 32)?;
    let phnum = u16_at(header, 56)?;
    if u16_at(header, 54)? != 56 || !(1..256).contains(&phnum) {
        return Err(SceError::BadProgramHeader);
    }
    let program_headers = (0..u64::from(phnum))
        .map(|i| read_program_header(data, base.saturating_add(phoff).saturating_add(i * 56)))
        .collect::<Result<Vec<_>, _>>()?;
    let header_end = phoff.saturating_add(u64::from(phnum) * 56);
    let end = program_headers
        .iter()
        .map(|p| p.offset.saturating_add(p.filesz))
        .fold(header_end, u64::max);
    if end > IMAGE_LIMIT {
        return Err(SceError::ImageTooLarge);
    }
    let segments = program_headers
        .iter()
        .filter(|p| p.kind == PT_LOAD || p.kind == PT_SCE_RELRO)
        .map(ProgramHeader::segment)
        .collect();
    if plain_elf {
        // An unpacked ELF already holds its segments at their file offsets.
        return Ok(SelfInfo {
            elf: span(data, 0, end)?.to_vec(),
            entry,
            segments,
            program_headers,
            self_segments: Vec::new(),
            missing: Vec::new(),
        });
    }
    let mut elf = vec![0; usize::try_from(end).map_err(|_| SceError::ImageTooLarge)?];
    place(&mut elf, 0, span(data, base, header_end)?)?;
    let mut covered = vec![(0, header_end)];
    let mut self_segments = Vec::new();
    for i in 0..count {
        let record = span(data, 32 + i * 32, 32)?;
        let segment = SelfSegment {
            flags: u64_at(record, 0)?,
            offset: u64_at(record, 8)?,
            size: u64_at(record, 16)?,
        };
        let memory = u64_at(record, 24)?;
        let (flags, offset, size) = (segment.flags, segment.offset, segment.size);
        self_segments.push(segment);
        if flags & SELF_HOLDS_SEGMENT == 0 {
            continue;
        }
        if flags & SELF_ENCRYPTED_OR_COMPRESSED != 0 {
            return Err(SceError::Encrypted);
        }
        let header = usize::try_from((flags >> 20) & 0xfff)
            .ok()
            .and_then(|index| program_headers.get(index))
            .ok_or(SceError::BadSegmentIndex)?;
        if size != header.filesz || size != memory {
            return Err(SceError::BlockedSegment);
        }
        place(&mut elf, header.offset, span(data, offset, size)?)?;
        covered.push((header.offset, header.offset.saturating_add(size)));
    }
    let mut missing = Vec::new();
    for (i, p) in program_headers.iter().enumerate() {
        let stop = p.offset.saturating_add(p.filesz);
        if p.filesz != 0 && !covered.iter().any(|&(a, z)| a <= p.offset && stop <= z) {
            missing.push(i);
            if matches!(
                p.kind,
                PT_LOAD | PT_DYNAMIC | PT_SCE_DYNLIBDATA | PT_SCE_RELRO
            ) {
                return Err(SceError::RequiredSegment(i));
            }
        }
    }
    Ok(SelfInfo {
        elf,
        entry,
        segments,
        program_headers,
        self_segments,
        missing,
    })
}

/// The bytes of a SELF file, which must hold something: an empty `eboot.bin` or prx (a copy that
/// failed, a placeholder made by hand) is named here instead of failing deep in the parser.
pub fn read_self_bytes(path: &Path) -> Result<Vec<u8>, SceError> {
    let data = fs::read(path).map_err(io_error(path))?;
    if data.is_empty() {
        return Err(SceError::EmptyFile(path.to_path_buf()));
    }
    Ok(data)
}

/// The SELF or ELF in `path`, parsed.
pub fn read_self(path: &Path) -> Result<SelfInfo, SceError> {
    parse_self(&read_self_bytes(path)?)
}

/// What a library or module id of the dynamic tags stands for: a name and a version.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Versioned {
    pub name: String,
    pub version: u16,
}

/// What a symbol name `NID#library#module` means in any module: the NID, and the library and
/// module that the local ids name in the module holding the symbol.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Identity {
    pub nid: String,
    pub library: Versioned,
    pub module: Versioned,
}

/// What a symbol name other than `NID#library#module` (such as `module_start`) means.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PlainNames {
    /// An error: `link_libc` links encoded names only.
    Refused,
    /// No identity: `link_modules` leaves such a symbol unlinked.
    Unlinkable,
}

/// The library and module ids the symbol names use, by local id, in the order the dynamic tags
/// first name them.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Ids {
    pub libraries: IndexMap<String, Versioned>,
    pub modules: IndexMap<String, Versioned>,
    plain: PlainNames,
}

impl Ids {
    /// What `name` means through these ids: `None` when it is not `NID#library#module`.
    pub fn resolve(&self, name: &str) -> Result<Option<Identity>, SceError> {
        let parts: Vec<&str> = name.split('#').collect();
        let [nid, library, module] = parts[..] else {
            return Ok(None);
        };
        let id = |ids: &IndexMap<String, Versioned>, key: &str| {
            ids.get(key)
                .cloned()
                .ok_or_else(|| SceError::UnknownId(key.to_owned()))
        };
        Ok(Some(Identity {
            nid: nid.to_owned(),
            library: id(&self.libraries, library)?,
            module: id(&self.modules, module)?,
        }))
    }

    /// The identity of a symbol name, by the module's `PlainNames` rule; `None` for no name.
    pub fn identity(&self, name: &str) -> Result<Option<Identity>, SceError> {
        if name.is_empty() {
            return Ok(None);
        }
        let identity = self.resolve(name)?;
        match self.plain {
            PlainNames::Refused => identity
                .ok_or_else(|| SceError::SymbolEncoding(name.to_owned()))
                .map(Some),
            PlainNames::Unlinkable => Ok(identity),
        }
    }
}

/// The local id of a library or module as symbol names spell it: `value` in base 64, most
/// significant digit first, over `A–Z a–z 0–9 + -`.
pub fn encode_id(value: u64) -> String {
    const ALPHABET: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
    let mut digits = vec![ALPHABET[(value & 63) as usize]];
    let mut rest = value >> 6;
    while rest != 0 {
        digits.push(ALPHABET[(rest & 63) as usize]);
        rest >>= 6;
    }
    digits
        .iter()
        .rev()
        .map(|&digit| char::from(digit))
        .collect()
}

/// One entry of a module's symbol table, with its name read.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Symbol {
    pub name: String,
    /// `st_info & 15`.
    pub kind: u8,
    /// `st_info >> 4`.
    pub binding: u8,
    pub section: u16,
    pub value: u64,
    pub size: u64,
}

/// One relocation of a module (`Elf64_Rela`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Relocation {
    pub target: u64,
    /// The low half of `r_info`.
    pub kind: u32,
    /// The symbol's index, the high half of `r_info`.
    pub symbol: u64,
    pub addend: i64,
}

/// What a relocation's symbol stands for, with the addend added.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Target<'a> {
    /// An absolute symbol (`SHN_ABS`): its value is the address itself.
    Absolute(i128),
    /// A symbol the module defines: an address relative to its load base.
    Defined(i128),
    /// A symbol the module imports.
    Undefined(&'a Symbol),
}

/// What an import is, in the probe's relocation records (1 and 2).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImportKind {
    Function = 1,
    Object = 2,
}

impl ImportKind {
    pub fn of(symbol: &Symbol) -> Self {
        if symbol.kind == STT_OBJECT {
            Self::Object
        } else {
            Self::Function
        }
    }

    /// Whether `addend` is one the probe supports: inside the first page of what the import
    /// names, and zero for a function.
    pub fn allows(self, addend: i64) -> bool {
        (0..4096).contains(&addend) && (self == Self::Object || addend == 0)
    }
}

/// A module read once: the container, the dynamic tags and the symbols. The relocations and the
/// ids are read when asked for, since not every caller wants them.
#[derive(Debug, Clone)]
pub struct SceModule {
    /// The file's name.
    pub file: String,
    /// SHA-256 of the file as it was read.
    pub sha256: [u8; 32],
    /// The file's size.
    pub bytes: u64,
    pub info: SelfInfo,
    /// The symbol table, in order (the relocations index it): exports, imports and the rest.
    pub symbols: Vec<Symbol>,
    /// The dynamic entries up to `DT_NULL`, in order.
    entries: Vec<(u64, u64)>,
    tags: HashMap<u64, u64>,
}

impl SceModule {
    /// Reads the module in `path` (a SELF or an unpacked ELF): its dynamic section up to
    /// `DT_NULL`, string table and symbol table (a multiple of 24 bytes).
    pub fn read(path: &Path) -> Result<Self, SceError> {
        let source = read_self_bytes(path)?;
        let info = parse_self(&source)?;
        let dynamic = header_in(&info, PT_DYNAMIC, "dynamic")?;
        let mut entries = Vec::new();
        for pos in (dynamic.offset..dynamic.offset.saturating_add(dynamic.filesz)).step_by(16) {
            let record = span(&info.elf, pos, 16)?;
            let tag = u64_at(record, 0)?;
            if tag == DT_NULL {
                break;
            }
            entries.push((tag, u64_at(record, 8)?));
        }
        let tags = entries.iter().copied().collect();
        let mut module = Self {
            file: file_name(path),
            sha256: Sha256::digest(&source).into(),
            bytes: source.len() as u64,
            info,
            symbols: Vec::new(),
            entries,
            tags,
        };
        module.symbols = module.read_symbols()?;
        Ok(module)
    }

    /// The first program header of `kind`.
    pub fn header(&self, kind: u32) -> Option<&ProgramHeader> {
        self.info.program_headers.iter().find(|p| p.kind == kind)
    }

    /// The value of dynamic tag `tag`.
    pub fn tag(&self, tag: u64) -> Option<u64> {
        self.tags.get(&tag).copied()
    }

    /// Whether `size` bytes at `address` lie inside one loadable segment.
    pub fn mapped(&self, address: u64, size: u64) -> bool {
        mapped(&self.info.segments, address, size)
    }

    /// The `PT_LOAD` headers, as `ProbeImage::patch_fs_loads` scans them.
    pub fn loads(&self) -> Vec<Segment> {
        self.info
            .program_headers
            .iter()
            .filter(|p| p.kind == PT_LOAD)
            .map(ProgramHeader::segment)
            .collect()
    }

    /// Symbol number `index`, as a relocation names it.
    pub fn symbol(&self, index: u64) -> Result<&Symbol, SceError> {
        usize::try_from(index)
            .ok()
            .and_then(|at| self.symbols.get(at))
            .ok_or(SceError::BadSymbolIndex(index))
    }

    /// The library blob (`PT_SCE_DYNLIBDATA`).
    fn blob(&self) -> Result<&[u8], SceError> {
        let library = header_in(&self.info, PT_SCE_DYNLIBDATA, "SCE dynlib data")?;
        span(&self.info.elf, library.offset, library.filesz)
    }

    fn strings<'a>(&self, blob: &'a [u8]) -> Result<&'a [u8], SceError> {
        table(blob, &self.tags, DT_SCE_STRTAB, DT_SCE_STRSZ)
    }

    fn read_symbols(&self) -> Result<Vec<Symbol>, SceError> {
        let blob = self.blob()?;
        let strings = self.strings(blob)?;
        let syms = table(blob, &self.tags, DT_SCE_SYMTAB, DT_SCE_SYMTABSZ)?;
        if !(syms.len() as u64).is_multiple_of(SYMBOL_SIZE) {
            return Err(SceError::BadSymbolTable);
        }
        let mut symbols = Vec::new();
        for pos in (0..syms.len() as u64).step_by(SYMBOL_SIZE as usize) {
            let record = span(syms, pos, SYMBOL_SIZE)?;
            let info = u8_at(record, 4)?;
            symbols.push(Symbol {
                name: name_at(strings, u64::from(u32_at(record, 0)?))?,
                kind: info & 15,
                binding: info >> 4,
                section: u16_at(record, 6)?,
                value: u64_at(record, 8)?,
                size: u64_at(record, 16)?,
            });
        }
        Ok(symbols)
    }

    /// The PLT relocations, then the others; each table a multiple of 24 bytes.
    pub fn relocations(&self) -> Result<Vec<Relocation>, SceError> {
        let blob = self.blob()?;
        let mut relocations = Vec::new();
        for (offset_tag, size_tag) in [
            (DT_SCE_JMPREL, DT_SCE_PLTRELSZ),
            (DT_SCE_RELA, DT_SCE_RELASZ),
        ] {
            let rela = table(blob, &self.tags, offset_tag, size_tag)?;
            if !(rela.len() as u64).is_multiple_of(RELOCATION_SIZE) {
                return Err(SceError::BadRelocationTable);
            }
            for pos in (0..rela.len() as u64).step_by(RELOCATION_SIZE as usize) {
                let record = span(rela, pos, RELOCATION_SIZE)?;
                let info = u64_at(record, 8)?;
                relocations.push(Relocation {
                    target: u64_at(record, 0)?,
                    kind: (info & 0xffff_ffff) as u32,
                    symbol: info >> 32,
                    addend: i64_at(record, 16)?,
                });
            }
        }
        Ok(relocations)
    }

    /// The library and module ids the dynamic tags name; `plain` says what a symbol name that is
    /// not `NID#library#module` means.
    pub fn ids(&self, plain: PlainNames) -> Result<Ids, SceError> {
        let blob = self.blob()?;
        let strings = self.strings(blob)?;
        let mut libraries = IndexMap::new();
        let mut modules = IndexMap::new();
        for &(tag, value) in &self.entries {
            let ids = if LIBRARY_TAGS.contains(&tag) {
                &mut libraries
            } else if MODULE_TAGS.contains(&tag) {
                &mut modules
            } else {
                continue;
            };
            let key = encode_id(value >> 48);
            let id = Versioned {
                name: name_at(strings, value & 0xffff_ffff)?,
                version: (value >> 32) as u16,
            };
            if ids.get(&key).is_some_and(|known| *known != id) {
                return Err(SceError::ConflictingIds);
            }
            ids.insert(key, id);
        }
        Ok(Ids {
            libraries,
            modules,
            plain,
        })
    }

    /// The names of the libraries the dynamic section needs (`DT_NEEDED`), in order.
    pub fn needed(&self) -> Result<Vec<String>, SceError> {
        let blob = self.blob()?;
        let strings = self.strings(blob)?;
        self.entries
            .iter()
            .filter(|&&(tag, _)| tag == DT_NEEDED)
            .map(|&(_, offset)| name_at(strings, offset))
            .collect()
    }

    /// The memory image: every loadable segment's file bytes at its address, the rest zero. The
    /// entry point must lie in a segment.
    pub fn memory_image(&self) -> Result<Vec<u8>, SceError> {
        let loads = &self.info.segments;
        let size = loads
            .iter()
            .map(|p| u128::from(p.vaddr) + u128::from(p.memsz))
            .max()
            .ok_or(SceError::MissingSegment("loadable"))?;
        if size > u128::from(IMAGE_LIMIT) || !self.mapped(self.info.entry, 1) {
            return Err(SceError::InvalidImage);
        }
        let mut image = vec![0; usize::try_from(size).map_err(|_| SceError::InvalidImage)?];
        for p in loads {
            if p.filesz > p.memsz {
                return Err(SceError::SegmentTooLarge);
            }
            place(
                &mut image,
                p.vaddr,
                span(&self.info.elf, p.offset, p.filesz)?,
            )?;
        }
        Ok(image)
    }

    /// The bytes of the function `s` defines, from the executable segment that holds it.
    pub fn code(&self, s: &Symbol) -> Result<&[u8], SceError> {
        let segment = self
            .info
            .program_headers
            .iter()
            .find(|p| {
                p.kind == PT_LOAD
                    && p.flags & PF_X != 0
                    && p.vaddr <= s.value
                    && s.value - p.vaddr < p.filesz
            })
            .ok_or(SceError::MissingSegment("executable"))?;
        let start = segment.offset.saturating_add(s.value - segment.vaddr);
        span(&self.info.elf, start, s.size)
    }

    /// What the symbol of `r` stands for, with the addend added.
    pub fn resolve(&self, r: &Relocation) -> Result<Target<'_>, SceError> {
        let symbol = self.symbol(r.symbol)?;
        let sum = i128::from(symbol.value) + i128::from(r.addend);
        Ok(if symbol.section == SHN_ABS {
            Target::Absolute(sum)
        } else if symbol.section != 0 {
            Target::Defined(sum)
        } else {
            Target::Undefined(symbol)
        })
    }
}

/// The first program header of `kind`.
pub(super) fn header_in<'a>(
    info: &'a SelfInfo,
    kind: u32,
    name: &'static str,
) -> Result<&'a ProgramHeader, SceError> {
    info.program_headers
        .iter()
        .find(|p| p.kind == kind)
        .ok_or(SceError::MissingSegment(name))
}

#[cfg(test)]
pub mod tests {
    use super::*;

    /// Little-endian fields as `struct.pack('<…')` lays them out: `(value, width in bytes)` each.
    pub(in crate::runner) fn pack(fields: &[(u64, usize)]) -> Vec<u8> {
        fields
            .iter()
            .flat_map(|&(value, width)| value.to_le_bytes()[..width].to_vec())
            .collect()
    }

    pub(in crate::runner) fn put(data: &mut [u8], at: usize, bytes: &[u8]) {
        data[at..at + bytes.len()].copy_from_slice(bytes);
    }

    /// `<16sHHIQQQIHHHHHH`: an x86-64 ELF header whose program headers follow it at offset 64.
    pub(in crate::runner) fn elf_header(e_type: u64, phnum: u64) -> Vec<u8> {
        let mut header = b"\x7fELF\x02\x01\x01\x09".to_vec();
        header.resize(16, 0);
        header.extend(pack(&[
            (e_type, 2),
            (62, 2),
            (1, 4),
            (0, 8),
            (64, 8),
            (0, 8),
            (0, 4),
            (64, 2),
            (56, 2),
            (phnum, 2),
            (0, 2),
            (0, 2),
            (0, 2),
        ]));
        header
    }

    /// `<IIQQQQQQ`: type, flags, offset, vaddr, paddr, filesz, memsz, align.
    pub(in crate::runner) fn program_header([kind, flags, rest @ ..]: [u64; 8]) -> Vec<u8> {
        let mut header = pack(&[(kind, 4), (flags, 4)]);
        for value in rest {
            header.extend(value.to_le_bytes());
        }
        header
    }

    /// `<QQQQ`: a SELF segment entry (flags, file offset, size, memory size).
    pub(in crate::runner) fn self_entry(fields: [u64; 4]) -> Vec<u8> {
        fields
            .iter()
            .flat_map(|value| value.to_le_bytes())
            .collect()
    }

    /// `tests/test_prepare.py::fixture`: a SELF whose one plaintext segment lands at file offset
    /// 0x1000 of the ELF while its PT_LOAD maps it at address 0.
    pub(in crate::runner) fn synthetic_self() -> Vec<u8> {
        let mut data = vec![0; 0x210];
        put(&mut data, 0, b"O\x15=\x1d");
        put(&mut data, 24, &pack(&[(1, 2)]));
        put(&mut data, 32, &self_entry([0x800, 0x200, 16, 16]));
        put(&mut data, 64, &elf_header(0xfe10, 1));
        put(
            &mut data,
            128,
            &program_header([1, 5, 0x1000, 0, 0, 16, 32, 0x1000]),
        );
        put(&mut data, 0x200, &(0..16).collect::<Vec<u8>>());
        data
    }

    /// `result` must be an error whose message contains `text` (the Python's `assertRaisesRegex`).
    #[track_caller]
    pub(in crate::runner) fn assert_fails<T: std::fmt::Debug>(
        result: Result<T, impl std::fmt::Display>,
        text: &str,
    ) {
        let message = result.unwrap_err().to_string();
        assert!(message.contains(text), "{message:?} lacks {text:?}");
    }

    #[test]
    fn unpacked_elf_has_the_same_headers_and_payload_as_self() {
        let expected = parse_self(&synthetic_self()).unwrap();
        let actual = parse_self(&expected.elf).unwrap();
        assert_eq!(actual.elf, expected.elf);
        assert_eq!(actual.entry, expected.entry);
        assert_eq!(actual.program_headers, expected.program_headers);
        assert_eq!(actual.segments, expected.segments);
        assert_eq!(actual.self_segments, []);
        assert_eq!(actual.missing, Vec::<usize>::new());
    }

    #[test]
    fn unpacked_elf_rejects_truncated_segments() {
        let elf = parse_self(&synthetic_self()).unwrap().elf;
        assert_fails(parse_self(&elf[..elf.len() - 1]), "out of bounds");
    }

    #[test]
    fn unpacked_elf_rejects_other_architectures_and_byte_orders() {
        let elf = parse_self(&synthetic_self()).unwrap().elf;
        for (offset, value) in [(4, 1), (5, 2), (18, 3)] {
            let mut bad = elf.clone();
            bad[offset] = value;
            assert_fails(parse_self(&bad), "little-endian x86-64 ELF");
        }
    }

    #[test]
    fn unpacked_elf_rejects_truncated_program_headers() {
        let elf = parse_self(&synthetic_self()).unwrap().elf;
        assert_fails(parse_self(&elf[..100]), "out of bounds");
    }

    #[test]
    fn unpacked_elf_checks_image_limit_before_allocating() {
        let mut elf = parse_self(&synthetic_self()).unwrap().elf;
        put(&mut elf, 64 + 8, &(512u64 * 1024 * 1024).to_le_bytes());
        assert_fails(parse_self(&elf), "512 MiB");
    }

    #[test]
    fn file_offset_is_not_virtual_address() {
        let info = parse_self(&synthetic_self()).unwrap();
        assert_eq!(info.elf[0x1000..0x1010], (0..16).collect::<Vec<u8>>());
        assert_eq!(info.program_headers[0].vaddr, 0);
        assert_eq!(info.missing, Vec::<usize>::new());
    }

    #[test]
    fn truncated_payload_rejected() {
        let data = synthetic_self();
        assert_fails(parse_self(&data[..data.len() - 1]), "out of bounds");
    }

    #[test]
    fn encrypted_or_compressed_payload_rejected() {
        for flag in [2u64, 8] {
            let mut data = synthetic_self();
            put(&mut data, 32, &(0x800 | flag).to_le_bytes());
            assert_fails(parse_self(&data), "encrypted/compressed");
        }
    }

    #[test]
    fn missing_required_segment_rejected() {
        let mut data = synthetic_self();
        put(&mut data, 32, &0u64.to_le_bytes());
        assert_fails(parse_self(&data), "required segment");
    }

    #[test]
    fn bad_program_header_index_rejected() {
        let mut data = synthetic_self();
        put(&mut data, 32, &0x10_0800u64.to_le_bytes());
        assert_fails(parse_self(&data), "segment index");
    }

    // The module reader: `dynamic` lays out the tables, the tests bend them.
    use crate::runner::packages::{dynamic, elf, Program, Sym};

    const EXPORT_LIBRARY: u64 = 0x6100_0013;
    const MODULE_INFO: u64 = 0x6100_000d;

    fn sym(name: &str, info: u8, section: u16, value: u64) -> Sym<'_> {
        Sym {
            name,
            info,
            section,
            value,
            size: 0,
        }
    }

    /// Writes a module with these tables, lets `bend` change its dynamic section, and reads it.
    fn read_with(
        ids: &[(u64, u64, &str, u64)],
        symbols: &[Sym],
        relocations: &[(u64, u32, u64, i64)],
        bend: impl FnOnce(&mut Vec<u8>),
    ) -> Result<SceModule, SceError> {
        let mut programs = dynamic(&[], ids, symbols, relocations);
        bend(&mut programs[0].data);
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("m.prx");
        std::fs::write(&path, elf(0xfe18, &programs)).unwrap();
        SceModule::read(&path)
    }

    /// Sets the value of dynamic tag `tag` (the first entry that has it).
    fn set_tag(dynamic: &mut [u8], tag: u64, value: u64) {
        let at = dynamic
            .chunks_exact(16)
            .position(|entry| entry[..8] == tag.to_le_bytes())
            .unwrap();
        put(dynamic, at * 16 + 8, &value.to_le_bytes());
    }

    fn tag_value(dynamic: &[u8], tag: u64) -> u64 {
        let entry = dynamic
            .chunks_exact(16)
            .find(|entry| entry[..8] == tag.to_le_bytes())
            .unwrap();
        u64::from_le_bytes(entry[8..].try_into().unwrap())
    }

    fn message<T: std::fmt::Debug>(result: Result<T, SceError>) -> String {
        result.unwrap_err().to_string()
    }

    #[test]
    fn stops_at_dt_null_for_every_stage() {
        let ids = [(EXPORT_LIBRARY, 1, "libc", 1)];
        let symbols = [sym("", 3, 1, 0), sym("f#A#B", 0x12, 1, 16)];
        let relocs = [(8, 8, 0, 4)];
        // Entries after DT_NULL that would break every table, and add an id.
        let module = read_with(&ids, &symbols, &relocs, |dynamic| {
            for (tag, value) in [
                (DT_SCE_STRTAB, 0xffffu64),
                (DT_SCE_SYMTABSZ, 7),
                (DT_SCE_PLTRELSZ, 7),
                (EXPORT_LIBRARY, 7 << 48),
            ] {
                dynamic.extend(tag.to_le_bytes());
                dynamic.extend(value.to_le_bytes());
            }
        })
        .unwrap();
        assert_eq!(module.symbols.len(), 2);
        assert_eq!(module.symbols[1].name, "f#A#B");
        assert_eq!(module.relocations().unwrap().len(), 1);
        assert_eq!(module.ids(PlainNames::Refused).unwrap().libraries.len(), 1);
        assert_ne!(module.tag(DT_SCE_STRTAB), Some(0xffff));
    }

    #[test]
    fn a_partial_symbol_table_is_named_by_every_stage() {
        let symbols = [sym("", 3, 1, 0), sym("f#A#B", 0x12, 1, 16)];
        let bend = |dynamic: &mut Vec<u8>| {
            let size = tag_value(dynamic, DT_SCE_SYMTABSZ);
            set_tag(dynamic, DT_SCE_SYMTABSZ, size - 1);
        };
        let error = read_with(&[], &symbols, &[], bend);
        assert!(message(error).contains("bad symbol table size"));
        // The linkers read modules through the same reader.
        let mut programs = dynamic(&[], &[], &symbols, &[]);
        bend(&mut programs[0].data);
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("m.prx");
        std::fs::write(&path, elf(0xfe18, &programs)).unwrap();
        let libc = crate::runner::link_libc::module(&path).unwrap_err();
        let modules = crate::runner::link_modules::module(&path).unwrap_err();
        for error in [libc.to_string(), modules.to_string()] {
            assert!(error.contains("bad symbol table size"), "{error}");
        }
    }

    #[test]
    fn a_partial_relocation_table_is_named() {
        let module = read_with(&[], &[sym("", 3, 1, 0)], &[(8, 8, 0, 4)], |dynamic| {
            let size = tag_value(dynamic, DT_SCE_PLTRELSZ);
            set_tag(dynamic, DT_SCE_PLTRELSZ, size - 1);
        })
        .unwrap();
        // Read on demand: the module itself is fine.
        assert!(message(module.relocations()).contains("bad relocation table size"));
    }

    #[test]
    fn conflicting_ids_are_refused() {
        let ids = [
            (EXPORT_LIBRARY, 1, "libc", 1),
            (EXPORT_LIBRARY, 1, "libkernel", 1),
        ];
        let module = read_with(&ids, &[sym("", 3, 1, 0)], &[], |_| {}).unwrap();
        let error = message(module.ids(PlainNames::Unlinkable));
        assert_eq!(error, "conflicting module/library IDs");
        // The same id named twice alike is fine.
        let ids = [ids[0], ids[0]];
        let module = read_with(&ids, &[sym("", 3, 1, 0)], &[], |_| {}).unwrap();
        assert_eq!(
            module.ids(PlainNames::Unlinkable).unwrap().libraries.len(),
            1
        );
    }

    #[test]
    fn plain_names_refused_or_unlinkable() {
        let ids = [(EXPORT_LIBRARY, 2, "libc", 3), (MODULE_INFO, 0, "libm", 1)];
        let module = read_with(&ids, &[sym("", 3, 1, 0)], &[], |_| {}).unwrap();
        let refused = module.ids(PlainNames::Refused).unwrap();
        let unlinkable = module.ids(PlainNames::Unlinkable).unwrap();
        let identity = refused.identity("nid#C#A").unwrap().unwrap();
        assert_eq!(identity.nid, "nid");
        assert_eq!(identity.library.name, "libc");
        assert_eq!(identity.library.version, 3);
        assert_eq!(identity.module.name, "libm");
        assert_eq!(unlinkable.identity("nid#C#A").unwrap(), Some(identity));
        assert_eq!(
            message(refused.identity("module_start")),
            "unsupported symbol encoding 'module_start'"
        );
        assert_eq!(unlinkable.identity("module_start").unwrap(), None);
        // A symbol without a name has no identity either way.
        assert_eq!(refused.identity("").unwrap(), None);
        assert_eq!(
            message(refused.identity("nid#Z#A")),
            "no library or module with id 'Z'"
        );
    }

    #[test]
    fn resolve_gives_absolute_defined_or_undefined() {
        let symbols = [
            sym("", 3, 1, 0),
            sym("abs", 0x12, SHN_ABS, 0x100),
            sym("def", 0x12, 1, 16),
            sym("undef", 0x12, 0, 0),
        ];
        let relocs = [(8, 1, 1, 8), (8, 1, 2, -4), (8, 1, 3, 12), (8, 1, 9, 0)];
        let module = read_with(&[], &symbols, &relocs, |_| {}).unwrap();
        let relocations = module.relocations().unwrap();
        assert_eq!(
            module.resolve(&relocations[0]).unwrap(),
            Target::Absolute(0x108)
        );
        assert_eq!(
            module.resolve(&relocations[1]).unwrap(),
            Target::Defined(12)
        );
        assert_eq!(
            module.resolve(&relocations[2]).unwrap(),
            Target::Undefined(&module.symbols[3])
        );
        assert_eq!(
            message(module.resolve(&relocations[3])),
            "relocation symbol 9 does not exist"
        );
    }

    #[test]
    fn import_kind_and_addend_rule() {
        let function = sym("f", 0x12, 0, 0);
        let object = sym("o", 0x11, 0, 0);
        let symbols: Vec<Symbol> = [function, object]
            .into_iter()
            .map(|s| Symbol {
                name: s.name.to_owned(),
                kind: s.info & 15,
                binding: s.info >> 4,
                section: s.section,
                value: s.value,
                size: s.size,
            })
            .collect();
        assert_eq!(ImportKind::of(&symbols[0]), ImportKind::Function);
        assert_eq!(ImportKind::of(&symbols[1]), ImportKind::Object);
        assert_eq!(ImportKind::Function as u64, 1);
        assert_eq!(ImportKind::Object as u64, 2);
        // A function's addend is zero; an object's stays in the first page.
        assert!(ImportKind::Function.allows(0));
        assert!(!ImportKind::Function.allows(1));
        assert!(ImportKind::Object.allows(0));
        assert!(ImportKind::Object.allows(4095));
        assert!(!ImportKind::Object.allows(4096));
        assert!(!ImportKind::Object.allows(-1));
        assert!(!ImportKind::Function.allows(-1));
    }

    /// A module whose program headers are `dynamic` (tables) and `loads` (`PT_LOAD`s).
    fn read_loads(tags: &[(u64, u64)], loads: Vec<Program>) -> Result<SceModule, SceError> {
        let ids = [(EXPORT_LIBRARY, 1, "libc", 1)];
        let mut programs: Vec<Program> = dynamic(tags, &ids, &[sym("", 3, 1, 0)], &[]).into();
        programs.extend(loads);
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("m.prx");
        std::fs::write(&path, elf(0xfe18, &programs)).unwrap();
        SceModule::read(&path)
    }

    fn load(flags: u32, vaddr: u64, memsz: u64, data: &[u8]) -> Program {
        Program {
            kind: PT_LOAD,
            flags,
            vaddr,
            memsz,
            align: 0x1000,
            data: data.to_vec(),
        }
    }

    #[test]
    fn needed_names_the_dt_needed_libraries() {
        // "libc" is the first string the fixture adds, at offset 1 of the string table.
        let module = read_loads(&[(DT_NEEDED, 1), (DT_NEEDED, 1)], vec![]).unwrap();
        assert_eq!(module.needed().unwrap(), ["libc", "libc"]);
        let module = read_loads(&[(DT_NEEDED, 0xffff)], vec![]).unwrap();
        assert!(message(module.needed()).contains("0xffff"));
        assert_eq!(read_loads(&[], vec![]).unwrap().needed().unwrap().len(), 0);
    }

    #[test]
    fn memory_image_puts_each_segment_at_its_address() {
        // The entry (0) lies in the first segment.
        let module = read_loads(&[], vec![load(5, 0, 16, &[1, 2, 3, 4])]).unwrap();
        let mut expected = vec![1, 2, 3, 4];
        expected.resize(16, 0);
        assert_eq!(module.memory_image().unwrap(), expected);
        let module = read_loads(&[], vec![]).unwrap();
        assert_eq!(message(module.memory_image()), "no loadable segment");
        // The entry must be mapped.
        let module = read_loads(&[], vec![load(5, 0x10, 16, &[1])]).unwrap();
        assert_eq!(message(module.memory_image()), "invalid memory image");
        let module = read_loads(&[], vec![load(5, 0, 2, &[1, 2, 3, 4])]).unwrap();
        assert_eq!(
            message(module.memory_image()),
            "segment file size exceeds memory size"
        );
    }

    #[test]
    fn code_is_read_from_the_executable_segment() {
        let mut bytes = vec![0; 0x20];
        bytes[0x11] = 0xc3;
        let module = read_loads(
            &[],
            vec![
                load(4, 0x1000, 0x20, &[0x90; 0x20]),
                load(5, 0x100, 0x20, &bytes),
            ],
        )
        .unwrap();
        let symbol = |value, size| Symbol {
            name: "f".to_owned(),
            kind: STT_FUNC,
            binding: 1,
            section: 1,
            value,
            size,
        };
        // The readable segment holds 0x90s; only the executable one counts.
        assert_eq!(module.code(&symbol(0x111, 1)).unwrap(), [0xc3]);
        assert_eq!(module.code(&symbol(0x110, 2)).unwrap(), [0, 0xc3]);
        assert_eq!(
            message(module.code(&symbol(0x1000, 1))),
            "no executable segment"
        );
        assert!(message(module.code(&symbol(0x11f, 8))).starts_with("out of bounds"));
    }
}
