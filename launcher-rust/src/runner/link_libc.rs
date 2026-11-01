//! The libc linker (Python `scripts/link_libc.py`): links the game's plaintext `libc.prx` after the
//! eboot into `boot-libc.bin` (`G3PROBE4`) and `libc-link.json`. Nothing reads them any more (the
//! probe loads the image of `link_modules`); they are kept for parity. Symbols match by NID and by
//! library and module identity, versions included, never by the dump's local ids or the NID alone.
//!
//! Also what `link_modules` shares: the linkers' view of a module and writing the outputs.

use std::collections::HashMap;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use indexmap::IndexMap;
use serde::Serialize;

use super::probe_image::{
    Binding, Extension, ImageError, ImageRelocation, LibcHeader, ProbeImage, TlsLayout, Version,
};
pub use super::sce::{encode_id, Identity, PlainNames, Relocation, Versioned};
use super::sce::{
    hex, hex_number, place, Ids, ImportKind, SceError, SceModule, Symbol, Target, DT_INIT,
    PT_SCE_PROCPARAM, PT_TLS, R_X86_64_64, R_X86_64_DTPMOD64, R_X86_64_GLOB_DAT,
    R_X86_64_JUMP_SLOT, R_X86_64_RELATIVE, STB_GLOBAL, STB_WEAK, STT_FUNC, STT_OBJECT, STT_SECTION,
};
use crate::core::fs::atomic_write;
use crate::core::python::ensure_ascii;

/// The libc's TLS module id in the probe (the eboot's is 1).
const LIBC_TLS_MODULE: u64 = 2;
/// The largest eboot TLS block the probe sets up.
const MAIN_TLS_LIMIT: u64 = 1024 * 1024;

/// Why a link failed. The texts are the Python's; where the Python stopped on a built-in error (a
/// missing tag, segment or id, a bad index, a value `struct` cannot pack) the message names it.
#[derive(Debug, thiserror::Error)]
pub enum LinkError {
    #[error(transparent)]
    Sce(#[from] SceError),
    #[error(transparent)]
    Image(#[from] ImageError),
    #[error("{}: {source}", path.display())]
    Io { path: PathBuf, source: io::Error },
    #[error("an import has no name")]
    UnnamedImport,
    #[error("value out of range")]
    OutOfRange,
    #[error("unsupported eboot TLS layout")]
    EbootTlsLayout,
    // `link_libc`.
    #[error("invalid libc segment size")]
    LibcSegmentSize,
    #[error("libc relocation outside image {0:#x}")]
    LibcRelocationOutside(u64),
    #[error("unsupported external TLS relocation")]
    LibcExternalTls,
    #[error("unsupported libc import addend")]
    LibcImportAddend,
    #[error("unsupported libc relocation type {0}")]
    LibcRelocationType(u32),
    #[error("export outside libc load segments")]
    LibcExportOutside,
    #[error("ambiguous libc export")]
    LibcAmbiguousExport,
    #[error("unsupported TLS layout")]
    LibcTlsLayout,
    // `link_modules`, after the module's file name.
    #[error("{0}: invalid segment size")]
    SegmentSize(String),
    #[error("{0}: unsupported TLS layout")]
    TlsLayout(String),
    #[error("{file}: relocation outside image {target:#x}")]
    RelocationOutside { file: String, target: u64 },
    #[error("{0}: unsupported external TLS relocation")]
    ExternalTls(String),
    #[error("{0}: unsupported import addend")]
    ImportAddend(String),
    #[error("{file}: unsupported relocation type {kind}")]
    RelocationType { file: String, kind: u32 },
    #[error("{0}: export outside load segments")]
    ExportOutside(String),
    #[error("{0}: ambiguous export")]
    AmbiguousExport(String),
}

/// A module as the linkers read it.
#[derive(Debug, Clone)]
pub struct Module {
    pub sce: SceModule,
    /// The PLT relocations, then the others.
    pub relocations: Vec<Relocation>,
    /// The library and module ids the symbol names use.
    pub ids: Ids,
    /// What each of `sce.symbols` is named by: `None` for a symbol without a name and, in
    /// `link_modules`, for a plain name.
    pub identities: Vec<Option<Identity>>,
}

impl Module {
    /// Reads the module in `path` (a SELF or an unpacked ELF) once, with the linkers' view of it:
    /// the symbols with their identities, the relocations and the ids.
    pub fn read(path: &Path, plain: PlainNames) -> Result<Self, LinkError> {
        let sce = SceModule::read(path)?;
        let ids = sce.ids(plain)?;
        let identities = sce
            .symbols
            .iter()
            .map(|s| ids.identity(&s.name))
            .collect::<Result<_, _>>()?;
        let relocations = sce.relocations()?;
        Ok(Self {
            sce,
            relocations,
            ids,
            identities,
        })
    }

    /// The identity of the symbol a relocation names.
    pub fn identity_of(&self, index: u64) -> Option<&Identity> {
        usize::try_from(index)
            .ok()
            .and_then(|index| self.identities.get(index))
            .and_then(Option::as_ref)
    }

    /// What the module exports, in symbol order: each function or object it defines, global or
    /// weak, named `NID#library#module`.
    pub fn exports(&self) -> impl Iterator<Item = (&Symbol, &Identity)> {
        self.sce
            .symbols
            .iter()
            .zip(&self.identities)
            .filter(|(symbol, _)| {
                symbol.section != 0
                    && matches!(symbol.binding, STB_GLOBAL | STB_WEAK)
                    && matches!(symbol.kind, STT_OBJECT | STT_FUNC)
            })
            .filter_map(|(symbol, identity)| Some((symbol, identity.as_ref()?)))
    }

    /// The TLS layout of this module when it is the eboot (`main_tls`), which must fit the probe:
    /// the template inside the eboot's `size` bytes, at most 1 MiB in all. All zero without a TLS
    /// segment.
    pub fn main_tls(&self, size: u64) -> Result<TlsLayout, LinkError> {
        let Some(p) = self.sce.header(PT_TLS) else {
            return Ok(TlsLayout::default());
        };
        if p.filesz > p.memsz || p.memsz > MAIN_TLS_LIMIT || p.vaddr.saturating_add(p.filesz) > size
        {
            return Err(LinkError::EbootTlsLayout);
        }
        Ok(TlsLayout {
            vaddr: p.vaddr,
            filesz: p.filesz,
            memsz: p.memsz,
            align: p.align,
        })
    }
}

/// `identity` of `name`, which must be `NID#library#module`.
fn encoded(identity: Option<Identity>, name: &str) -> Result<Identity, SceError> {
    identity.ok_or_else(|| SceError::SymbolEncoding(name.to_owned()))
}

/// Reads the module in `path` (a SELF or an unpacked ELF) once; every named symbol must be
/// `NID#library#module`.
pub fn module(path: &Path) -> Result<Module, LinkError> {
    Module::read(path, PlainNames::Refused)
}

fn io_error(path: &Path) -> impl FnOnce(io::Error) -> LinkError + '_ {
    move |source| LinkError::Io {
        path: path.to_path_buf(),
        source,
    }
}

pub(super) fn write(path: &Path, bytes: &[u8]) -> Result<(), LinkError> {
    atomic_write(path, bytes).map_err(io_error(path))
}

/// Writes `report` as the Python's `json.dumps(report, indent=2)` does (`ensure_ascii`), plus a
/// newline.
pub(super) fn write_json(path: &Path, report: &impl Serialize) -> Result<(), LinkError> {
    let text = serde_json::to_string_pretty(report)
        .map_err(|error| io_error(path)(io::Error::other(error)))?;
    let mut ascii = ensure_ascii(&text);
    ascii.push('\n');
    write(path, ascii.as_bytes())
}

/// An import of `symbol`'s binding to the libc.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct SymbolBinding {
    pub import_name: String,
    #[serde(serialize_with = "hex_number")]
    pub address: u64,
    /// 1 a function, 2 an object.
    pub kind: u64,
}

/// What `link` did: `libc-link.json`, in the Python's key order.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct LinkReport {
    /// Where the libc starts in the image.
    #[serde(serialize_with = "hex_number")]
    pub base: u64,
    /// The libc's memory size.
    pub size: u64,
    /// SHA-256 of `libc.prx`, hex.
    pub sha256: String,
    /// The libc's initializer, in the image.
    #[serde(serialize_with = "hex_number")]
    pub init: u64,
    /// How many imports the libc's exports serve.
    pub bindings: usize,
    pub tls_module_id: u64,
    /// How many `mov rax, fs:[0]` of the eboot now read `gs:`.
    pub fs_loads_patched: usize,
    pub main_tls: TlsLayout,
    /// The libc's TLS module-id relocations.
    pub tls_relocations: usize,
    pub tls_template_bytes: u64,
    pub tls_memory_bytes: u64,
    /// The import names: the eboot's, then the libc's.
    pub imports: Vec<String>,
    /// The libc's relocations by type, in the order the types first appear.
    pub relocation_counts: IndexMap<u32, u64>,
    pub symbol_bindings: Vec<SymbolBinding>,
}

/// The number of the import of `symbol`, added after the others when it is new. It keeps the
/// eboot's spelling (its local ids, for the host resolver) only when the eboot knows its library
/// and module, versions included; else it is `libc:<name>`.
fn import(
    identities: &mut IndexMap<Identity, usize>,
    names: &mut Vec<String>,
    symbol: &Symbol,
    identity: Option<&Identity>,
    libraries: &HashMap<&Versioned, &str>,
    modules: &HashMap<&Versioned, &str>,
) -> Result<usize, LinkError> {
    let identity = identity.ok_or(LinkError::UnnamedImport)?;
    if let Some(&index) = identities.get(identity) {
        return Ok(index);
    }
    let name = match (
        libraries.get(&identity.library),
        modules.get(&identity.module),
    ) {
        (Some(library), Some(module)) => format!("{}#{library}#{module}", identity.nid),
        _ => format!("libc:{}", symbol.name),
    };
    let index = names.len();
    identities.insert(identity.clone(), index);
    names.push(name);
    Ok(index)
}

/// Links `<game>/eboot.bin`, as `<out>/boot.bin` holds it, with `<game>/sce_module/libc.prx` into
/// `<out>/boot-libc.bin` and `<out>/libc-link.json`.
pub fn link(game: &Path, out: &Path) -> Result<LinkReport, LinkError> {
    let main = module(&game.join("eboot.bin"))?;
    let libc = module(&game.join("sce_module").join("libc.prx"))?;
    let path = out.join("boot.bin");
    let data = fs::read(&path).map_err(io_error(&path))?;
    let mut boot = ProbeImage::decode(&data, Version::Prepared)?;
    let size = boot.image.len() as u64;
    let base = boot.next_base();
    let libsize = boot.map_module(&libc.sce, base, || LinkError::LibcSegmentSize)?;
    let mut identities = IndexMap::new();
    for (index, name) in boot.names.iter().enumerate() {
        identities.insert(encoded(main.ids.resolve(name)?, name)?, index);
    }
    let main_libraries: HashMap<&Versioned, &str> = main
        .ids
        .libraries
        .iter()
        .map(|(key, id)| (id, key.as_str()))
        .collect();
    let main_modules: HashMap<&Versioned, &str> = main
        .ids
        .modules
        .iter()
        .map(|(key, id)| (id, key.as_str()))
        .collect();
    let mut tls_relocations = 0;
    for r in &libc.relocations {
        if !libc.sce.mapped(r.target, 8) {
            return Err(LinkError::LibcRelocationOutside(r.target));
        }
        // `target` lies below `libsize`, which fits the image limit with `base`.
        let target = base + r.target;
        match r.kind {
            R_X86_64_RELATIVE => boot.relocate(target, i128::from(base) + i128::from(r.addend)),
            R_X86_64_DTPMOD64 => {
                // Only the libc's own TLS block: its offset is already in the next word.
                let symbol = libc.sce.symbol(r.symbol)?;
                if !symbol.name.is_empty() || symbol.kind != STT_SECTION || r.addend != 0 {
                    return Err(LinkError::LibcExternalTls);
                }
                place(&mut boot.image, target, &LIBC_TLS_MODULE.to_le_bytes())?;
                tls_relocations += 1;
            }
            R_X86_64_64 | R_X86_64_GLOB_DAT | R_X86_64_JUMP_SLOT => match libc.sce.resolve(r)? {
                Target::Absolute(sum) => {
                    let value = u64::try_from(sum).map_err(|_| LinkError::OutOfRange)?;
                    place(&mut boot.image, target, &value.to_le_bytes())?;
                }
                Target::Defined(sum) => boot.relocate(target, i128::from(base) + sum),
                Target::Undefined(symbol) => {
                    let kind = ImportKind::of(symbol);
                    if !kind.allows(r.addend) {
                        return Err(LinkError::LibcImportAddend);
                    }
                    let index = import(
                        &mut identities,
                        &mut boot.names,
                        symbol,
                        libc.identity_of(r.symbol),
                        &main_libraries,
                        &main_modules,
                    )?;
                    boot.relocations.push(ImageRelocation {
                        target,
                        kind: kind as u64,
                        value: index as i128,
                        addend: r.addend,
                    });
                }
            },
            kind => return Err(LinkError::LibcRelocationType(kind)),
        }
    }
    let mut exports: HashMap<&Identity, &Symbol> = HashMap::new();
    for (symbol, identity) in libc.exports() {
        if !libc.sce.mapped(symbol.value, symbol.size.max(1)) {
            return Err(LinkError::LibcExportOutside);
        }
        if exports.insert(identity, symbol).is_some() {
            return Err(LinkError::LibcAmbiguousExport);
        }
    }
    let bindings: Vec<Binding> = identities
        .iter()
        .filter_map(|(identity, &import)| {
            let symbol = exports.get(identity)?;
            Some(Binding {
                import: import as u64,
                address: base + symbol.value,
                kind: ImportKind::of(symbol) as u64,
            })
        })
        .collect();
    let fs_loads_patched = boot.patch_fs_loads(&main.sce.loads(), 0);
    let main_tls = main.main_tls(size)?;
    let tls = libc
        .sce
        .header(PT_TLS)
        .ok_or(SceError::MissingSegment("TLS"))?;
    if tls.filesz > tls.memsz || !libc.sce.mapped(tls.vaddr, tls.memsz) {
        return Err(LinkError::LibcTlsLayout);
    }
    let procparam = main
        .sce
        .header(PT_SCE_PROCPARAM)
        .ok_or(SceError::MissingSegment("procparam"))?;
    let init = libc.sce.tag(DT_INIT).ok_or(SceError::MissingTag(DT_INIT))?;
    let init = base.checked_add(init).ok_or(LinkError::OutOfRange)?;
    let symbol_bindings = bindings
        .iter()
        .filter_map(|binding| {
            let name = usize::try_from(binding.import).ok()?;
            Some(SymbolBinding {
                import_name: boot.names.get(name)?.clone(),
                address: binding.address,
                kind: binding.kind,
            })
        })
        .collect();
    let binding_count = bindings.len();
    boot.extension = Extension::Libc(LibcHeader {
        base,
        size: libsize,
        init,
        tls_address: base + tls.vaddr,
        tls_memsz: tls.memsz,
        tls_filesz: tls.filesz,
        procparam: procparam.vaddr,
        main_tls,
        bindings,
    });
    write(&out.join("boot-libc.bin"), &boot.encode()?)?;
    let mut relocation_counts = IndexMap::new();
    for r in &libc.relocations {
        *relocation_counts.entry(r.kind).or_insert(0) += 1;
    }
    let report = LinkReport {
        base,
        size: libsize,
        sha256: hex(&libc.sce.sha256),
        init,
        bindings: binding_count,
        tls_module_id: LIBC_TLS_MODULE,
        fs_loads_patched,
        main_tls,
        tls_relocations,
        tls_template_bytes: tls.filesz,
        tls_memory_bytes: tls.memsz,
        imports: boot.names,
        relocation_counts,
        symbol_bindings,
    };
    write_json(&out.join("libc-link.json"), &report)?;
    Ok(report)
}

#[cfg(test)]
mod tests {
    use std::fs;

    use serde_json::{json, Value};
    use sha2::{Digest, Sha256};

    use super::*;
    use crate::runner::packages::{dynamic, elf, package, self_fixture, Program, Sym};
    use crate::runner::sce::parse_self;

    const EXPORT_LIBRARY: u64 = 0x6100_0013;
    const MODULE_INFO: u64 = 0x6100_000d;
    /// `mov rax, fs:[0]`.
    const FS_LOAD: [u8; 9] = [0x64, 0x48, 0x8b, 0x04, 0x25, 0, 0, 0, 0];

    fn versioned(name: &str, version: u16) -> Versioned {
        Versioned {
            name: name.to_owned(),
            version,
        }
    }

    /// `result` must be an error whose message contains `text` (the Python's `assertRaisesRegex`).
    #[track_caller]
    fn assert_fails<T: std::fmt::Debug>(result: Result<T, LinkError>, text: &str) {
        let message = result.unwrap_err().to_string();
        assert!(message.contains(text), "{message:?} lacks {text:?}");
    }

    #[test]
    fn module_accepts_unpacked_elf_with_the_same_exports() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("libc.prx");
        let source = self_fixture();
        fs::write(&path, &source).unwrap();
        let expected = module(&path).unwrap();
        let elf = parse_self(&source).unwrap().elf;
        fs::write(&path, &elf).unwrap();
        let actual = module(&path).unwrap();
        assert_eq!(actual.sce.symbols, expected.sce.symbols);
        assert_eq!(
            actual.sce.info.program_headers,
            expected.sce.info.program_headers
        );
        assert_eq!(actual.relocations, expected.relocations);
        assert_eq!(actual.sce.sha256, <[u8; 32]>::from(Sha256::digest(&elf)));
    }

    #[test]
    fn module_reads_the_file_once_and_reports_its_hash() {
        let data = self_fixture();
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("libc.prx");
        fs::write(&path, &data).unwrap();
        let info = module(&path).unwrap();
        assert_eq!(info.sce.sha256, <[u8; 32]>::from(Sha256::digest(&data)));
        assert_eq!(info.sce.info.missing, Vec::<usize>::new());
        let identity = Identity {
            nid: "bzQExy189ZI".to_owned(),
            library: versioned("libc", 1),
            module: versioned("libc", 1),
        };
        assert_eq!(info.identities[0], Some(identity));
        assert_eq!(info.relocations, []);
    }

    /// `LinkTests.fixture`'s libc: it exports `fixture#C#A`, whose library C is libc `version` and
    /// module A libc 1, holds a TLS template at 128 and the relocations `relocs` (target, type,
    /// symbol, addend).
    fn libc(version: u64, relocs: &[(u64, u32, u64, i64)]) -> Vec<u8> {
        let mut code = vec![0x31, 0xc0, 0xc3];
        code.resize(16, 0);
        code.push(0xc3);
        code.resize(128, 0);
        let symbols = [
            Sym {
                name: "",
                info: 3,
                section: 1,
                value: 0,
                size: 0,
            },
            Sym {
                name: "fixture#C#A",
                info: 0x12,
                section: 1,
                value: 16,
                size: 1,
            },
        ];
        let ids = [
            (EXPORT_LIBRARY, 2, "libc", version),
            (MODULE_INFO, 0, "libc", 1),
        ];
        let mut programs = vec![
            Program {
                kind: 1,
                flags: 5,
                vaddr: 0,
                memsz: 4096,
                align: 0x1000,
                data: code,
            },
            Program {
                kind: 7,
                flags: 4,
                vaddr: 128,
                memsz: 32,
                align: 16,
                data: vec![0; 4],
            },
        ];
        programs.extend(dynamic(&[(12, 0)], &ids, &symbols, relocs));
        elf(0xfe18, &programs)
    }

    /// `LinkTests.fixture`'s eboot: its library and module `q` are libc 1, its procparam is at 256,
    /// and `extra` are more program headers.
    fn eboot(extra: Vec<Program>) -> Vec<u8> {
        let mut programs = vec![Program {
            kind: 0x6100_0001,
            flags: 4,
            vaddr: 256,
            memsz: 0,
            align: 8,
            data: Vec::new(),
        }];
        programs.extend(extra);
        let ids = [
            (EXPORT_LIBRARY, 42, "libc", 1),
            (MODULE_INFO, 42, "libc", 1),
        ];
        programs.extend(dynamic(&[], &ids, &[], &[]));
        elf(0xfe10, &programs)
    }

    /// `LinkTests.run_link`: links `eboot` and `libc` over a `boot.bin` that maps `code` and
    /// imports `fixture#q#q`; returns `boot-libc.bin` and `libc-link.json`.
    fn run_link(eboot: &[u8], libc: &[u8], code: &[u8]) -> Result<(Vec<u8>, Value), LinkError> {
        let dir = tempfile::tempdir().unwrap();
        let game = dir.path().join("game");
        let out = dir.path().join("out");
        fs::create_dir_all(game.join("sce_module")).unwrap();
        fs::create_dir_all(&out).unwrap();
        fs::write(game.join("eboot.bin"), eboot).unwrap();
        fs::write(game.join("sce_module").join("libc.prx"), libc).unwrap();
        let boot = package(code, &[], &["fixture#q#q"], Some(1));
        fs::write(out.join("boot.bin"), boot).unwrap();
        link(&game, &out)?;
        let data = fs::read(out.join("boot-libc.bin")).unwrap();
        let report = serde_json::from_slice(&fs::read(out.join("libc-link.json")).unwrap());
        Ok((data, report.unwrap()))
    }

    /// The memory image at the end of `boot-libc.bin`.
    fn image(data: &[u8]) -> &[u8] {
        let size = u64::from_le_bytes(data[8..16].try_into().unwrap()) as usize;
        &data[data.len() - size..]
    }

    /// A `PT_LOAD` of the eboot over the first 64 bytes of `boot.bin`'s image.
    fn eboot_load(flags: u32) -> Program {
        Program {
            kind: 1,
            flags,
            vaddr: 0,
            memsz: 64,
            align: 0x1000,
            data: vec![0; 64],
        }
    }

    #[test]
    fn different_local_ids_bind_same_identity() {
        let (data, report) = run_link(&eboot(Vec::new()), &libc(1, &[]), &[0xc3]).unwrap();
        assert_eq!(&data[..8], b"G3PROBE4");
        assert_eq!(
            report["symbol_bindings"],
            json!([{"import_name": "fixture#q#q", "address": "0x10010", "kind": 1}])
        );
    }

    #[test]
    fn same_nid_wrong_library_version_does_not_bind() {
        let (_, report) = run_link(&eboot(Vec::new()), &libc(2, &[]), &[0xc3]).unwrap();
        assert_eq!(report["bindings"], 0);
    }

    #[test]
    fn tls_module_id_is_literal_not_image_pointer() {
        let libc = libc(1, &[(64, 16, 0, 0)]);
        let (data, report) = run_link(&eboot(Vec::new()), &libc, &[0xc3]).unwrap();
        let slot = &image(&data)[65536 + 64..65536 + 72];
        assert_eq!(u64::from_le_bytes(slot.try_into().unwrap()), 2);
        assert_eq!(report["tls_relocations"], 1);
    }

    #[test]
    fn unsupported_tls_symbol_rejected() {
        let libc = libc(1, &[(64, 16, 1, 0)]);
        assert_fails(
            run_link(&eboot(Vec::new()), &libc, &[0xc3]),
            "unsupported external TLS",
        );
    }

    #[test]
    fn relocation_outside_segments_rejected() {
        let libc = libc(1, &[(4092, 8, 0, 0)]);
        assert_fails(
            run_link(&eboot(Vec::new()), &libc, &[0xc3]),
            "relocation outside image",
        );
    }

    #[test]
    fn fs_thread_pointer_loads_are_rewritten_to_gs() {
        let tls = Program {
            kind: 7,
            flags: 4,
            vaddr: 2048,
            memsz: 48,
            align: 16,
            data: Vec::new(),
        };
        let eboot = eboot(vec![eboot_load(5), tls]);
        let code = [&FS_LOAD[..], &[0x90], &FS_LOAD[..], &[0xc3]].concat();
        let (data, report) = run_link(&eboot, &libc(1, &[]), &code).unwrap();
        let image = image(&data);
        assert_eq!(image[0], 0x65);
        assert_eq!(image[10], 0x65);
        assert_eq!(report["fs_loads_patched"], 2);
        assert_eq!(
            report["main_tls"],
            json!({"vaddr": 2048, "filesz": 0, "memsz": 48, "align": 16})
        );
        let main_tls: Vec<u64> = data[56 + 64..56 + 96]
            .chunks_exact(8)
            .map(|word| u64::from_le_bytes(word.try_into().unwrap()))
            .collect();
        assert_eq!(main_tls, [2048, 0, 48, 16]);
    }

    #[test]
    fn fs_loads_outside_executable_segments_are_kept() {
        let eboot = eboot(vec![eboot_load(6)]);
        let (data, report) = run_link(&eboot, &libc(1, &[]), &FS_LOAD).unwrap();
        assert_eq!(image(&data)[0], 0x64);
        assert_eq!(report["fs_loads_patched"], 0);
    }
}
