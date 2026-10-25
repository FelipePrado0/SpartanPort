//! The module linker (Python `scripts/link_modules.py`), successor of `link_libc` for several
//! modules: links the eboot with the game's bundled system modules (by default `libc.prx`, then
//! `libSceFios2.prx`) into `boot-linked.bin` (`G3PROBE5`), the image the probe loads, and
//! `link.json`. Each module goes after the previous one on a 64 KiB boundary. All of them share one
//! import table, so the C runtime can override any function (host contracts first) and otherwise
//! binds the native export.
//!
//! Exports match by NID, library and module identity. Imports of `libSceLibcInternal` are served
//! by the libc's export of the same NID: the game does not ship that library, which exposes the
//! same functions.

use std::collections::HashMap;
use std::fs;
use std::path::Path;

use indexmap::IndexMap;
use serde::Serialize;

use super::link_libc::{write, write_json, LinkError, Module};
use super::probe_image::{
    Binding, Extension, ImageRelocation, LinkedHeader, ModuleRecord, ProbeImage, TlsLayout, Version,
};
use super::sce::{
    hex, hex_number, place, Identity, ImportKind, PlainNames, SceError, Target, DT_INIT,
    PT_SCE_PROCPARAM, PT_TLS, R_X86_64_64, R_X86_64_DTPMOD64, R_X86_64_GLOB_DAT,
    R_X86_64_JUMP_SLOT, R_X86_64_RELATIVE, STB_WEAK, STT_SECTION,
};

pub const DEFAULT_MODULES: [&str; 2] = ["libc.prx", "libSceFios2.prx"];
/// The library whose imports the libc's exports serve.
const LIBC_INTERNAL: &str = "libSceLibcInternal";
/// The TLS module id of the first module with a TLS segment; each next one takes the next id (the
/// eboot's is 1).
const FIRST_TLS_MODULE: u64 = 2;

/// The name the host tables key an import by, `NID#library` (the eboot's own ids, such as `#c#C`,
/// differ per game); `fallback` for an import without an identity.
pub fn canonical(identity: Option<&Identity>, fallback: Option<&str>) -> Option<String> {
    match identity {
        Some(identity) => Some(format!("{}#{}", identity.nid, identity.library.name)),
        None => fallback.map(str::to_owned),
    }
}

/// Reads the module in `path` (a SELF or an unpacked ELF) once; a plain symbol name (such as
/// `module_start`) gets no identity.
pub fn module(path: &Path) -> Result<Module, LinkError> {
    Module::read(path, PlainNames::Unlinkable)
}

/// One linked module (`link.json`'s `modules`).
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct LinkedModule {
    /// The name `link` was given.
    pub file: String,
    /// Where the module starts in the image.
    #[serde(serialize_with = "hex_number")]
    pub base: u64,
    /// Its memory size.
    pub size: u64,
    /// Its initializer in the image (`DT_INIT`; its base when it has none).
    #[serde(serialize_with = "hex_number")]
    pub init: u64,
    /// Its TLS template in the image; 0 without a TLS segment.
    #[serde(serialize_with = "hex_number")]
    pub tls_address: u64,
    pub tls_memsz: u64,
    pub tls_filesz: u64,
    /// Its TLS module id; 0 without a TLS segment.
    pub tls_module: u64,
    pub tls_relocations: usize,
    /// How many symbols it exports.
    pub exports: usize,
    /// SHA-256 of the file, hex.
    pub sha256: String,
}

impl LinkedModule {
    /// The module as `boot-linked.bin` stores it.
    fn record(&self) -> ModuleRecord {
        ModuleRecord {
            base: self.base,
            size: self.size,
            init: self.init,
            tls_address: self.tls_address,
            tls_memsz: self.tls_memsz,
            tls_filesz: self.tls_filesz,
            tls_module: self.tls_module,
        }
    }
}

/// What `link` did: `link.json`, in the Python's key order.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct LinkReport {
    pub modules: Vec<LinkedModule>,
    /// How many imports a module's export serves.
    pub bindings: usize,
    /// How many imports there are: the eboot's, then the modules'.
    pub imports: usize,
    /// How many `mov rax, fs:[0]` now read `gs:`, in the eboot and the modules.
    pub fs_loads_patched: usize,
    pub main_tls: TlsLayout,
    /// The imports no module exports, left to the host runtime.
    pub unresolved_imports: Vec<String>,
}

/// An export: its address in the image and its kind (1 a function, 2 an object).
#[derive(Debug, Clone, Copy)]
struct Export {
    address: u64,
    kind: u64,
}

/// The number of the import of `identity`, added after the others under its canonical name when
/// it is new.
fn import(
    identities: &mut IndexMap<Option<Identity>, usize>,
    names: &mut Vec<String>,
    identity: Option<&Identity>,
) -> Result<usize, LinkError> {
    if let Some(&index) = identities.get(&identity.cloned()) {
        return Ok(index);
    }
    let name = canonical(identity, None).ok_or(LinkError::UnnamedImport)?;
    let index = names.len();
    identities.insert(identity.cloned(), index);
    names.push(name);
    Ok(index)
}

/// The export that serves `identity`: its own or, for `libSceLibcInternal`, the libc's only export
/// of the same NID.
fn served(
    identity: &Identity,
    exports: &HashMap<Identity, Export>,
    by_nid: &HashMap<(String, String), Vec<Export>>,
) -> Option<Export> {
    if let Some(&export) = exports.get(identity) {
        return Some(export);
    }
    if identity.library.name != LIBC_INTERNAL {
        return None;
    }
    match by_nid
        .get(&(identity.nid.clone(), "libc".to_owned()))?
        .as_slice()
    {
        [only] => Some(*only),
        _ => None,
    }
}

/// Links `<game>/eboot.bin`, as `<out>/boot.bin` holds it, with `<game>/sce_module/<module>` for
/// each of `modules`, in order, into `<out>/boot-linked.bin` and `<out>/link.json`.
pub fn link(game: &Path, out: &Path, modules: &[&str]) -> Result<LinkReport, LinkError> {
    let main = module(&game.join("eboot.bin"))?;
    let path = out.join("boot.bin");
    let data = fs::read(&path).map_err(|source| LinkError::Io {
        path: path.clone(),
        source,
    })?;
    let mut boot = ProbeImage::decode(&data, Version::Prepared)?;
    let size = boot.image.len() as u64;
    let mut identities = IndexMap::new();
    let mut names = Vec::new();
    for (index, name) in boot.names.iter().enumerate() {
        let identity = main.ids.resolve(name)?;
        names.push(canonical(identity.as_ref(), Some(name)).unwrap_or_else(|| name.clone()));
        identities.insert(identity, index);
    }
    boot.names = names;
    let mut exports: HashMap<Identity, Export> = HashMap::new();
    let mut by_nid: HashMap<(String, String), Vec<Export>> = HashMap::new();
    let mut table = Vec::new();
    let mut base = boot.next_base();
    let mut fs_loads_patched = boot.patch_fs_loads(&main.sce.loads(), 0);
    let mut tls_module = FIRST_TLS_MODULE;
    for &file in modules {
        let m = module(&game.join("sce_module").join(file))?;
        let modsize = boot.map_module(&m.sce, base, || LinkError::SegmentSize(file.to_owned()))?;
        let tls = m.sce.header(PT_TLS);
        let mut module_id = 0;
        if let Some(tls) = tls {
            if tls.filesz > tls.memsz || !m.sce.mapped(tls.vaddr, tls.memsz) {
                return Err(LinkError::TlsLayout(file.to_owned()));
            }
            module_id = tls_module;
            tls_module += 1;
        }
        let mut tls_relocations = 0;
        for r in &m.relocations {
            if !m.sce.mapped(r.target, 8) {
                return Err(LinkError::RelocationOutside {
                    file: file.to_owned(),
                    target: r.target,
                });
            }
            // `target` lies below `modsize`, which fits the image limit with `base`.
            let target = base + r.target;
            match r.kind {
                R_X86_64_RELATIVE => {
                    boot.relocate(target, i128::from(base) + i128::from(r.addend));
                }
                R_X86_64_DTPMOD64 => {
                    let symbol = m.sce.symbol(r.symbol)?;
                    if !symbol.name.is_empty()
                        || symbol.kind != STT_SECTION
                        || r.addend != 0
                        || module_id == 0
                    {
                        return Err(LinkError::ExternalTls(file.to_owned()));
                    }
                    place(&mut boot.image, target, &module_id.to_le_bytes())?;
                    tls_relocations += 1;
                }
                R_X86_64_64 | R_X86_64_GLOB_DAT | R_X86_64_JUMP_SLOT => match m.sce.resolve(r)? {
                    Target::Absolute(sum) => {
                        let value = u64::try_from(sum).map_err(|_| LinkError::OutOfRange)?;
                        place(&mut boot.image, target, &value.to_le_bytes())?;
                    }
                    Target::Defined(sum) => boot.relocate(target, i128::from(base) + sum),
                    Target::Undefined(symbol) => {
                        let identity = m.identity_of(r.symbol);
                        if identity.is_none() && symbol.binding == STB_WEAK {
                            // A weak undefined symbol (module_start, module_stop) stays null.
                            place(&mut boot.image, target, &0u64.to_le_bytes())?;
                            continue;
                        }
                        let kind = ImportKind::of(symbol);
                        if !kind.allows(r.addend) {
                            return Err(LinkError::ImportAddend(file.to_owned()));
                        }
                        let index = import(&mut identities, &mut boot.names, identity)?;
                        boot.relocations.push(ImageRelocation {
                            target,
                            kind: kind as u64,
                            value: index as i128,
                            addend: r.addend,
                        });
                    }
                },
                kind => {
                    return Err(LinkError::RelocationType {
                        file: file.to_owned(),
                        kind,
                    })
                }
            }
        }
        let mut count = 0;
        for (symbol, identity) in m.exports() {
            if !m.sce.mapped(symbol.value, symbol.size.max(1)) {
                return Err(LinkError::ExportOutside(file.to_owned()));
            }
            if exports.contains_key(identity) {
                return Err(LinkError::AmbiguousExport(file.to_owned()));
            }
            let export = Export {
                address: base + symbol.value,
                kind: ImportKind::of(symbol) as u64,
            };
            exports.insert(identity.clone(), export);
            by_nid
                .entry((identity.nid.clone(), identity.library.name.clone()))
                .or_default()
                .push(export);
            count += 1;
        }
        fs_loads_patched += boot.patch_fs_loads(&m.sce.loads(), base);
        let init = base
            .checked_add(m.sce.tag(DT_INIT).unwrap_or(0))
            .ok_or(LinkError::OutOfRange)?;
        table.push(LinkedModule {
            file: file.to_owned(),
            base,
            size: modsize,
            init,
            tls_address: tls.map_or(0, |tls| base + tls.vaddr),
            tls_memsz: tls.map_or(0, |tls| tls.memsz),
            tls_filesz: tls.map_or(0, |tls| tls.filesz),
            tls_module: module_id,
            tls_relocations,
            exports: count,
            sha256: hex(&m.sce.sha256),
        });
        base = boot.next_base();
    }
    let mut bindings = Vec::new();
    let mut unresolved_imports = Vec::new();
    for (identity, &import) in &identities {
        match identity
            .as_ref()
            .and_then(|identity| served(identity, &exports, &by_nid))
        {
            Some(export) => bindings.push(Binding {
                import: import as u64,
                address: export.address,
                kind: export.kind,
            }),
            None => unresolved_imports.extend(boot.names.get(import).cloned()),
        }
    }
    let main_tls = main.main_tls(size)?;
    let procparam = main
        .sce
        .header(PT_SCE_PROCPARAM)
        .ok_or(SceError::MissingSegment("procparam"))?;
    let binding_count = bindings.len();
    boot.extension = Extension::Linked(LinkedHeader {
        procparam: procparam.vaddr,
        main_tls,
        modules: table.iter().map(LinkedModule::record).collect(),
        bindings,
    });
    write(&out.join("boot-linked.bin"), &boot.encode()?)?;
    let report = LinkReport {
        modules: table,
        bindings: binding_count,
        imports: boot.names.len(),
        fs_loads_patched,
        main_tls,
        unresolved_imports,
    };
    write_json(&out.join("link.json"), &report)?;
    Ok(report)
}

#[cfg(test)]
mod tests {
    use std::fs;

    use sha2::{Digest, Sha256};

    use super::*;
    use crate::runner::link_libc::{Identity, Versioned};
    use crate::runner::packages::self_fixture;
    use crate::runner::sce::parse_self;

    fn libc_1() -> Versioned {
        Versioned {
            name: "libc".to_owned(),
            version: 1,
        }
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
            library: libc_1(),
            module: libc_1(),
        };
        assert_eq!(info.identities[0], Some(identity));
    }
}
