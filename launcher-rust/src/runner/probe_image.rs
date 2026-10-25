//! The probe images: `boot.bin` (`G3PROBE2`, which `prepare` writes and the linkers read),
//! `boot-libc.bin` (`G3PROBE4`, `link_libc`'s) and `boot-linked.bin` (`G3PROBE5`, the image the
//! probe loads, which `isa_rewrite` reads). Each is the same header (`<8s6Q`: magic, image size,
//! entry, segment, relocation and name counts, flags), its version's extension, the segments, the
//! import names, the relocations and the memory image, little-endian, with nothing after it.

use serde::Serialize;

use super::sce::{i64_at, place, span, u64_at, SceError, SceModule, Segment, IMAGE_LIMIT, PF_X};

const HEADER_SIZE: u64 = 56;
const RELOCATION_SIZE: u64 = 32;
/// Each import name is stored NUL-padded to this size.
const IMPORT_NAME_SIZE: usize = 128;
/// `mov rax, fs:[0]`: how the eboot and the modules read their thread pointer (initial-exec TLS).
const FS_LOAD: [u8; 9] = [0x64, 0x48, 0x8b, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00];
/// The `gs` segment prefix that replaces the `fs` one (0x64).
const GS_PREFIX: u8 = 0x65;

/// Which probe image a file is.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Version {
    /// `boot.bin` (`G3PROBE2`), as `prepare` writes it.
    Prepared,
    /// `boot-libc.bin` (`G3PROBE4`).
    Libc,
    /// `boot-linked.bin` (`G3PROBE5`).
    Linked,
}

impl Version {
    fn magic(self) -> &'static [u8; 8] {
        match self {
            Self::Prepared => b"G3PROBE2",
            Self::Libc => b"G3PROBE4",
            Self::Linked => b"G3PROBE5",
        }
    }

    fn file(self) -> &'static str {
        match self {
            Self::Prepared => "boot.bin",
            Self::Libc => "boot-libc.bin",
            Self::Linked => "boot-linked.bin",
        }
    }

    // The texts of the script that reads or writes each version; nothing reads a `G3PROBE4`, which
    // is named as `isa_rewrite.py` names a `G3PROBE5`.

    fn wrong_magic(self) -> &'static str {
        match self {
            Self::Prepared => "linker expects freshly prepared G3PROBE2",
            Self::Libc => "not a G3PROBE4 image",
            Self::Linked => "not a G3PROBE5 image",
        }
    }

    fn name_too_long(self) -> &'static str {
        match self {
            Self::Prepared => "import name too long",
            Self::Libc | Self::Linked => "name too long",
        }
    }
}

/// Why an image could not be read or written.
#[derive(Debug, thiserror::Error)]
pub enum ImageError {
    #[error(transparent)]
    Sce(#[from] SceError),
    #[error("{}", .0.wrong_magic())]
    Magic(Version),
    #[error("import name {index} of {} is not UTF-8", .version.file())]
    NameEncoding { version: Version, index: u64 },
    #[error("unexpected boot trailer")]
    Trailer,
    #[error("{}", .0.name_too_long())]
    NameTooLong(Version),
    /// A relocation value `struct.pack` cannot store as an `int64`.
    #[error("value out of range")]
    OutOfRange,
    #[error("linked image exceeds probe limit")]
    TooLarge,
}

/// A relocation (`<QQqq`): kind 0 stores the load base plus `value` at `target`; 1 and 2 store the
/// address of import number `value` (a function, an object) plus `addend`. `value` must fit an
/// `i64` when the image is encoded, as the Python's `struct.pack` checks then.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ImageRelocation {
    pub target: u64,
    pub kind: u64,
    pub value: i128,
    pub addend: i64,
}

/// An import a module's export serves (`<3Q`): the import's number, the export's address in the
/// image and its kind (1 a function, 2 an object).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Binding {
    pub import: u64,
    pub address: u64,
    pub kind: u64,
}

impl Binding {
    fn words(&self) -> [u64; 3] {
        [self.import, self.address, self.kind]
    }

    fn from_words([import, address, kind]: [u64; 3]) -> Self {
        Self {
            import,
            address,
            kind,
        }
    }
}

/// The eboot's TLS segment (`main_tls`), all zero when it has none.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize)]
pub struct TlsLayout {
    pub vaddr: u64,
    pub filesz: u64,
    pub memsz: u64,
    pub align: u64,
}

impl TlsLayout {
    fn words(&self) -> [u64; 4] {
        [self.vaddr, self.filesz, self.memsz, self.align]
    }

    fn from_words([vaddr, filesz, memsz, align]: [u64; 4]) -> Self {
        Self {
            vaddr,
            filesz,
            memsz,
            align,
        }
    }
}

/// A linked module (`<7Q`): where it starts in the image, its memory size, its initializer, its
/// TLS template (address, memory and file sizes) and its TLS module id.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ModuleRecord {
    pub base: u64,
    pub size: u64,
    pub init: u64,
    pub tls_address: u64,
    pub tls_memsz: u64,
    pub tls_filesz: u64,
    pub tls_module: u64,
}

impl ModuleRecord {
    fn words(&self) -> [u64; 7] {
        [
            self.base,
            self.size,
            self.init,
            self.tls_address,
            self.tls_memsz,
            self.tls_filesz,
            self.tls_module,
        ]
    }

    fn from_words(
        [base, size, init, tls_address, tls_memsz, tls_filesz, tls_module]: [u64; 7],
    ) -> Self {
        Self {
            base,
            size,
            init,
            tls_address,
            tls_memsz,
            tls_filesz,
            tls_module,
        }
    }
}

/// What `boot-libc.bin` holds after the header (`<8Q`, the main TLS, the bindings): the libc's
/// place, size, initializer and TLS template in the image, and the eboot's procparam and TLS.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LibcHeader {
    pub base: u64,
    pub size: u64,
    pub init: u64,
    pub tls_address: u64,
    pub tls_memsz: u64,
    pub tls_filesz: u64,
    pub procparam: u64,
    pub main_tls: TlsLayout,
    pub bindings: Vec<Binding>,
}

/// What `boot-linked.bin` holds after the header: the eboot's procparam and TLS, then the modules
/// and the bindings, each list after its count.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LinkedHeader {
    pub procparam: u64,
    pub main_tls: TlsLayout,
    pub modules: Vec<ModuleRecord>,
    pub bindings: Vec<Binding>,
}

/// What a version holds between the header and the segments.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Extension {
    None,
    Libc(LibcHeader),
    Linked(LinkedHeader),
}

impl Extension {
    fn version(&self) -> Version {
        match self {
            Self::None => Version::Prepared,
            Self::Libc(_) => Version::Libc,
            Self::Linked(_) => Version::Linked,
        }
    }
}

/// A probe image; `extension` says its version.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProbeImage {
    pub entry: u64,
    /// `boot.bin`'s capabilities (1: the libc's `_init_env` is `RET`), which the linkers keep.
    pub flags: u64,
    /// Each segment's address, memory size and flags.
    pub segments: Vec<[u64; 3]>,
    pub names: Vec<String>,
    pub relocations: Vec<ImageRelocation>,
    pub image: Vec<u8>,
    pub extension: Extension,
}

/// The little-endian words of `record`.
fn words<const N: usize>(record: &[u8]) -> Result<[u64; N], SceError> {
    let mut words = [0; N];
    for (at, word) in (0..).step_by(8).zip(words.iter_mut()) {
        *word = u64_at(record, at)?;
    }
    Ok(words)
}

/// Little-endian `u64`s, as `struct.pack('<…Q')` lays them out.
fn push_words(data: &mut Vec<u8>, values: &[u64]) {
    for value in values {
        data.extend_from_slice(&value.to_le_bytes());
    }
}

/// Reads an image from the front.
struct Reader<'a> {
    data: &'a [u8],
    pos: u64,
}

impl<'a> Reader<'a> {
    /// The next `count` records of `size` bytes, which must all be there.
    fn table(&mut self, count: u64, size: u64) -> Result<&'a [u8], SceError> {
        let bytes = count.checked_mul(size).ok_or_else(|| SceError::Truncated {
            offset: self.pos,
            size: count.saturating_mul(size),
            len: self.data.len() as u64,
        })?;
        let table = span(self.data, self.pos, bytes)?;
        // `span` checked that the table ends inside the data.
        self.pos += bytes;
        Ok(table)
    }

    fn words<const N: usize>(&mut self) -> Result<[u64; N], SceError> {
        words(self.table(1, 8 * N as u64)?)
    }

    /// The next `count` records of `N` words.
    fn records<const N: usize>(&mut self, count: u64) -> Result<Vec<[u64; N]>, SceError> {
        self.table(count, 8 * N as u64)?
            .chunks_exact(8 * N)
            .map(words)
            .collect()
    }
}

impl ProbeImage {
    /// Reads an image of version `expect`: the magic first, then every table, which must fit, and
    /// nothing after the memory image.
    pub fn decode(bytes: &[u8], expect: Version) -> Result<Self, ImageError> {
        let mut reader = Reader {
            data: bytes,
            pos: 0,
        };
        let header = reader.table(1, HEADER_SIZE)?;
        if span(header, 0, 8)? != expect.magic() {
            return Err(ImageError::Magic(expect));
        }
        let [size, entry, segment_count, relocation_count, name_count, flags] =
            words(span(header, 8, 48)?)?;
        let extension = match expect {
            Version::Prepared => Extension::None,
            Version::Libc => {
                let [base, size, init, tls_address, tls_memsz, tls_filesz, bindings, procparam] =
                    reader.words()?;
                let main_tls = TlsLayout::from_words(reader.words()?);
                let bindings = reader.records(bindings)?;
                Extension::Libc(LibcHeader {
                    base,
                    size,
                    init,
                    tls_address,
                    tls_memsz,
                    tls_filesz,
                    procparam,
                    main_tls,
                    bindings: bindings.into_iter().map(Binding::from_words).collect(),
                })
            }
            Version::Linked => {
                let [procparam] = reader.words()?;
                let main_tls = TlsLayout::from_words(reader.words()?);
                let [modules] = reader.words()?;
                let modules = reader.records(modules)?;
                let [bindings] = reader.words()?;
                let bindings = reader.records(bindings)?;
                Extension::Linked(LinkedHeader {
                    procparam,
                    main_tls,
                    modules: modules.into_iter().map(ModuleRecord::from_words).collect(),
                    bindings: bindings.into_iter().map(Binding::from_words).collect(),
                })
            }
        };
        let segments = reader.records(segment_count)?;
        let names = reader
            .table(name_count, IMPORT_NAME_SIZE as u64)?
            .chunks_exact(IMPORT_NAME_SIZE)
            .zip(0..)
            .map(|(record, index)| {
                let name = record.split(|&b| b == 0).next().unwrap_or_default();
                std::str::from_utf8(name)
                    .map(str::to_owned)
                    .map_err(|_| ImageError::NameEncoding {
                        version: expect,
                        index,
                    })
            })
            .collect::<Result<_, _>>()?;
        let relocations = reader
            .table(relocation_count, RELOCATION_SIZE)?
            .chunks_exact(RELOCATION_SIZE as usize)
            .map(|record| {
                Ok(ImageRelocation {
                    target: u64_at(record, 0)?,
                    kind: u64_at(record, 8)?,
                    value: i128::from(i64_at(record, 16)?),
                    addend: i64_at(record, 24)?,
                })
            })
            .collect::<Result<_, SceError>>()?;
        let image = reader.table(1, size)?.to_vec();
        if reader.pos != bytes.len() as u64 {
            return Err(ImageError::Trailer);
        }
        Ok(Self {
            entry,
            flags,
            segments,
            names,
            relocations,
            image,
            extension,
        })
    }

    /// The file of the image, in the version its extension says.
    pub fn encode(&self) -> Result<Vec<u8>, ImageError> {
        let version = self.extension.version();
        let mut data = version.magic().to_vec();
        push_words(
            &mut data,
            &[
                self.image.len() as u64,
                self.entry,
                self.segments.len() as u64,
                self.relocations.len() as u64,
                self.names.len() as u64,
                self.flags,
            ],
        );
        match &self.extension {
            Extension::None => {}
            Extension::Libc(libc) => {
                push_words(
                    &mut data,
                    &[
                        libc.base,
                        libc.size,
                        libc.init,
                        libc.tls_address,
                        libc.tls_memsz,
                        libc.tls_filesz,
                        libc.bindings.len() as u64,
                        libc.procparam,
                    ],
                );
                push_words(&mut data, &libc.main_tls.words());
                for binding in &libc.bindings {
                    push_words(&mut data, &binding.words());
                }
            }
            Extension::Linked(linked) => {
                push_words(&mut data, &[linked.procparam]);
                push_words(&mut data, &linked.main_tls.words());
                push_words(&mut data, &[linked.modules.len() as u64]);
                for module in &linked.modules {
                    push_words(&mut data, &module.words());
                }
                push_words(&mut data, &[linked.bindings.len() as u64]);
                for binding in &linked.bindings {
                    push_words(&mut data, &binding.words());
                }
            }
        }
        for segment in &self.segments {
            push_words(&mut data, segment);
        }
        for name in &self.names {
            if name.len() >= IMPORT_NAME_SIZE {
                return Err(ImageError::NameTooLong(version));
            }
            let start = data.len();
            data.extend_from_slice(name.as_bytes());
            data.resize(start + IMPORT_NAME_SIZE, 0);
        }
        for r in &self.relocations {
            let value = i64::try_from(r.value).map_err(|_| ImageError::OutOfRange)?;
            push_words(&mut data, &[r.target, r.kind]);
            data.extend_from_slice(&value.to_le_bytes());
            data.extend_from_slice(&r.addend.to_le_bytes());
        }
        data.extend_from_slice(&self.image);
        Ok(data)
    }

    /// A relocation that stores the probe's load base plus `value` at `target`.
    pub fn relocate(&mut self, target: u64, value: i128) {
        self.relocations.push(ImageRelocation {
            target,
            kind: 0,
            value,
            addend: 0,
        });
    }

    /// Where the next module goes: the image's end, rounded up to the 64 KiB boundary each linked
    /// module starts on.
    pub fn next_base(&self) -> u64 {
        (self.image.len() as u64).saturating_add(0xffff) & !0xffff
    }

    /// Maps `module`'s loadable segments at `base` of the image, which grows to their end, and
    /// returns the module's memory size. `size_error` is the error for a segment whose file size
    /// exceeds its memory size.
    pub fn map_module<E: From<SceError> + From<ImageError>>(
        &mut self,
        module: &SceModule,
        base: u64,
        size_error: impl Fn() -> E,
    ) -> Result<u64, E> {
        let info = &module.info;
        let size = info
            .segments
            .iter()
            .map(|p| p.vaddr.saturating_add(p.memsz))
            .max()
            .ok_or(SceError::MissingSegment("loadable"))?;
        let end = base.saturating_add(size);
        if end > IMAGE_LIMIT {
            return Err(ImageError::TooLarge.into());
        }
        let end = usize::try_from(end).map_err(|_| ImageError::TooLarge)?;
        self.image.resize(end, 0);
        for p in &info.segments {
            if p.filesz > p.memsz {
                return Err(size_error());
            }
            // `vaddr` lies below `size`, which fits the image limit with `base`.
            place(
                &mut self.image,
                base + p.vaddr,
                span(&info.elf, p.offset, p.filesz)?,
            )?;
            self.segments
                .push([base + p.vaddr, p.memsz, u64::from(p.flags)]);
        }
        Ok(size)
    }

    /// Rewrites every `mov rax, fs:[0]` (`FS_LOAD`) inside the executable segments of `loads` (a
    /// module's `PT_LOAD` headers, the module mapped at `base`) to read `gs:[0]`, and returns how
    /// many it rewrote: glibc owns FS on Linux, and the runtime points GS at each guest thread's
    /// TCB.
    pub fn patch_fs_loads(&mut self, loads: &[Segment], base: u64) -> usize {
        let image = &mut self.image;
        let mut patched = 0;
        for p in loads.iter().filter(|p| p.flags & PF_X != 0) {
            let len = image.len();
            let clamp = |at: u64| usize::try_from(at).map_or(len, |at| at.min(len));
            let start = base.saturating_add(p.vaddr);
            let end = clamp(start.saturating_add(p.filesz));
            let mut at = clamp(start);
            while let Some(found) = image.get(at..end).and_then(find_fs_load) {
                let hit = at + found;
                if let Some(prefix) = image.get_mut(hit) {
                    *prefix = GS_PREFIX;
                }
                patched += 1;
                at = hit + FS_LOAD.len();
            }
        }
        patched
    }
}

fn find_fs_load(window: &[u8]) -> Option<usize> {
    window
        .windows(FS_LOAD.len())
        .position(|bytes| bytes == FS_LOAD.as_slice())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::runner::packages::package;

    const VERSIONS: [Version; 3] = [Version::Prepared, Version::Libc, Version::Linked];
    /// `mov rax, fs:[0]`.
    const FS: [u8; 9] = [0x64, 0x48, 0x8b, 0x04, 0x25, 0, 0, 0, 0];

    #[track_caller]
    fn assert_fails<T: std::fmt::Debug>(result: Result<T, ImageError>, text: &str) {
        let message = result.unwrap_err().to_string();
        assert!(message.contains(text), "{message:?} lacks {text:?}");
    }

    fn main_tls() -> TlsLayout {
        TlsLayout {
            vaddr: 0x800,
            filesz: 16,
            memsz: 48,
            align: 16,
        }
    }

    fn module_record() -> ModuleRecord {
        ModuleRecord {
            base: 0x10000,
            size: 0x2000,
            init: 0x10010,
            tls_address: 0x10080,
            tls_memsz: 32,
            tls_filesz: 4,
            tls_module: 2,
        }
    }

    /// An image of `version` with two segments, two names (one not ASCII), a relocation of each
    /// kind and the version's extension.
    fn sample(version: Version) -> ProbeImage {
        let binding = Binding {
            import: 1,
            address: 0x10010,
            kind: 2,
        };
        let extension = match version {
            Version::Prepared => Extension::None,
            Version::Libc => Extension::Libc(LibcHeader {
                base: 0x10000,
                size: 0x2000,
                init: 0x10010,
                tls_address: 0x10080,
                tls_memsz: 32,
                tls_filesz: 4,
                procparam: 256,
                main_tls: main_tls(),
                bindings: vec![binding],
            }),
            Version::Linked => Extension::Linked(LinkedHeader {
                procparam: 256,
                main_tls: main_tls(),
                modules: vec![module_record()],
                bindings: vec![binding],
            }),
        };
        ProbeImage {
            entry: 0x10,
            flags: 1,
            segments: vec![[0, 0x1000, 5], [0x1000, 0x40, 6]],
            names: vec!["fixture#q#q".to_owned(), "libc:é".to_owned()],
            relocations: vec![
                ImageRelocation {
                    target: 8,
                    kind: 0,
                    value: -16,
                    addend: 0,
                },
                ImageRelocation {
                    target: 16,
                    kind: 2,
                    value: 1,
                    addend: 8,
                },
            ],
            image: (0..0x1040u32).map(|i| i as u8).collect(),
            extension,
        }
    }

    fn words(data: &[u8]) -> Vec<u64> {
        data.chunks_exact(8)
            .map(|word| u64::from_le_bytes(word.try_into().unwrap()))
            .collect()
    }

    #[test]
    fn round_trips_every_version() {
        for (version, magic) in VERSIONS
            .into_iter()
            .zip([b"G3PROBE2", b"G3PROBE4", b"G3PROBE5"])
        {
            let image = sample(version);
            let bytes = image.encode().unwrap();
            assert_eq!(&bytes[..8], magic);
            // `<8s6Q`: image size, entry, segment, relocation and name counts, flags.
            assert_eq!(words(&bytes[8..56]), [0x1040, 0x10, 2, 2, 2, 1]);
            assert!(bytes.ends_with(&image.image));
            let decoded = ProbeImage::decode(&bytes, version).unwrap();
            assert_eq!(decoded, image);
            assert_eq!(decoded.encode().unwrap(), bytes);
        }
    }

    #[test]
    fn decode_checks_the_magic_before_anything() {
        let texts = [
            (
                Version::Prepared,
                "linker expects freshly prepared G3PROBE2",
            ),
            (Version::Libc, "not a G3PROBE4 image"),
            (Version::Linked, "not a G3PROBE5 image"),
        ];
        for (expect, text) in texts {
            for other in VERSIONS.into_iter().filter(|&other| other != expect) {
                let mut bytes = sample(other).encode().unwrap();
                // Counts no file could hold, and a trailer: the magic is still what is named.
                for at in [8, 24, 32, 40] {
                    bytes[at..at + 8].copy_from_slice(&u64::MAX.to_le_bytes());
                }
                bytes.push(0);
                let error = ProbeImage::decode(&bytes, expect).unwrap_err();
                assert_eq!(error.to_string(), text);
            }
        }
        // The probe's first format, which no stage reads.
        let old = package(&[0xc3], &[], &[], None);
        assert_fails(
            ProbeImage::decode(&old, Version::Prepared),
            "linker expects freshly prepared G3PROBE2",
        );
    }

    #[test]
    fn decode_refuses_a_trailer_and_non_utf8_names() {
        let files = ["boot.bin", "boot-libc.bin", "boot-linked.bin"];
        for (version, file) in VERSIONS.into_iter().zip(files) {
            let bytes = sample(version).encode().unwrap();
            let trailer = [&bytes[..], b"\0"].concat();
            assert_fails(
                ProbeImage::decode(&trailer, version),
                "unexpected boot trailer",
            );
            assert_fails(
                ProbeImage::decode(&bytes[..bytes.len() - 1], version),
                "out of bounds",
            );
            // The second name's `é` loses its first byte: a lone continuation byte.
            let mut bad = bytes.clone();
            let at = bad
                .windows(2)
                .position(|pair| pair == "é".as_bytes())
                .unwrap();
            bad[at] = b'x';
            assert_fails(
                ProbeImage::decode(&bad, version),
                &format!("import name 1 of {file} is not UTF-8"),
            );
        }
    }

    #[test]
    fn name_of_128_bytes_is_refused_with_each_versions_text() {
        let texts = ["import name too long", "name too long", "name too long"];
        for (version, text) in VERSIONS.into_iter().zip(texts) {
            let mut image = sample(version);
            image.names = vec!["n".repeat(127)];
            let bytes = image.encode().unwrap();
            assert_eq!(ProbeImage::decode(&bytes, version).unwrap(), image);
            image.names = vec!["n".repeat(128)];
            assert_eq!(image.encode().unwrap_err().to_string(), text);
        }
    }

    #[test]
    fn value_past_i64_is_refused() {
        for version in VERSIONS {
            for value in [i128::from(i64::MAX) + 1, i128::from(i64::MIN) - 1] {
                let mut image = sample(version);
                image.relocate(0x20, value);
                assert_fails(image.encode(), "value out of range");
            }
            let mut image = sample(version);
            image.relocate(0x20, i128::from(i64::MIN));
            let decoded = ProbeImage::decode(&image.encode().unwrap(), version).unwrap();
            assert_eq!(
                decoded.relocations.last(),
                Some(&ImageRelocation {
                    target: 0x20,
                    kind: 0,
                    value: i128::from(i64::MIN),
                    addend: 0,
                })
            );
        }
    }

    #[test]
    fn decodes_the_python_package_fixture() {
        let code = [0x31, 0xc0, 0xc3];
        let relocs = [(8, 0, 16, 0), (16, 1, 1, 0)];
        let names = ["fixture#q#q", "bzQExy189ZI#C#A"];
        let bytes = package(&code, &relocs, &names, Some(1));
        let boot = ProbeImage::decode(&bytes, Version::Prepared).unwrap();
        assert_eq!((boot.entry, boot.flags), (0, 1));
        assert_eq!(boot.segments, [[0, 4096, 5]]);
        assert_eq!(boot.names, names);
        let relocations: Vec<_> = relocs
            .iter()
            .map(|&(target, kind, value, addend)| ImageRelocation {
                target,
                kind,
                value: i128::from(value),
                addend,
            })
            .collect();
        assert_eq!(boot.relocations, relocations);
        assert_eq!(boot.image.len(), 4096);
        assert_eq!(&boot.image[..3], code);
        assert_eq!(boot.extension, Extension::None);
        assert_eq!(boot.encode().unwrap(), bytes);
    }

    #[test]
    fn linked_module_records_are_56_bytes() {
        let mut image = sample(Version::Linked);
        let Extension::Linked(header) = &mut image.extension else {
            panic!("{:?}", image.extension)
        };
        header.modules.clear();
        let without = image.encode().unwrap().len();
        let Extension::Linked(header) = &mut image.extension else {
            panic!("{:?}", image.extension)
        };
        header.modules.push(module_record());
        let bytes = image.encode().unwrap();
        assert_eq!(bytes.len() - without, 56);
        // After the header, the procparam and the main TLS: the count, then the record's 7 words.
        assert_eq!(words(&bytes[56..96]), [256, 0x800, 16, 48, 16]);
        assert_eq!(
            words(&bytes[96..160]),
            [1, 0x10000, 0x2000, 0x10010, 0x10080, 32, 4, 2]
        );
        let decoded = ProbeImage::decode(&bytes, Version::Linked).unwrap();
        assert_eq!(decoded.extension, image.extension);
    }

    #[test]
    fn patch_fs_loads_only_in_executable_loads() {
        let load = |vaddr, filesz, flags| Segment {
            offset: 0,
            vaddr,
            filesz,
            memsz: filesz,
            flags,
        };
        let mut image = sample(Version::Prepared);
        image.image = vec![0x90; 0x100];
        let base = 0x40;
        // Before the module, twice in a row in its code, in its data, across the end of its
        // second code segment's file bytes.
        for at in [0, 0x40, 0x49, 0x60, 0x8a] {
            image.image[at..at + 9].copy_from_slice(&FS);
        }
        let loads = [
            load(0, 0x20, 5),
            load(0x20, 0x20, 6),
            load(0x40, 0x10, 5),
            // Past the image: nothing to scan.
            load(0x1000, 0x10, 5),
        ];
        assert_eq!(image.patch_fs_loads(&loads, base), 2);
        let prefixes: Vec<u8> = [0, 0x40, 0x49, 0x60, 0x8a]
            .iter()
            .map(|&at| image.image[at])
            .collect();
        assert_eq!(prefixes, [0x64, 0x65, 0x65, 0x64, 0x64]);
        assert_eq!(image.patch_fs_loads(&loads, base), 0);
    }
}
