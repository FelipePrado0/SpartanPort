// Read-only PS4 PKG filesystem bridge. Uses LGPL-3.0 LibOrbisPkg (maxton).
// Byte-stream protocol: op:u32, path_bytes:u32, offset:i64, count:u32, UTF-8 path.
// Reply: errno:i32, payload_bytes:u32, payload. No files are extracted.
using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using LibOrbisPkg.PFS;
using LibOrbisPkg.PKG;
using LibOrbisPkg.Util;

class PkgBridge {
    static PfsReader inner;
    static readonly object gate = new object();
    static int Main(string[] args) {
        try {
            if (args.Length != 2) throw new ArgumentException("Usage: gow3-pkg.exe PACKAGE PIPE_NAME");
            using (var packageStream = new FileStream(args[0], FileMode.Open, FileAccess.Read, FileShare.Read))
            using (var map = MemoryMappedFile.CreateFromFile(packageStream, null, 0, MemoryMappedFileAccess.Read, HandleInheritability.None, true)) {
                Pkg pkg;
                using (var stream = map.CreateViewStream(0, 0, MemoryMappedFileAccess.Read)) pkg = new PkgReader(stream).ReadPkg();
                using (var view = map.CreateViewAccessor((long)pkg.Header.pfs_image_offset, (long)pkg.Header.pfs_image_size, MemoryMappedFileAccess.Read)) {
                    var outer = new PfsReader(view, pkg.Header.pfs_flags, pkg.GetEkpfs());
                    inner = new PfsReader(new PFSCReader(outer.GetFile("pfs_image.dat").GetView()));
                    // Package metadata lives outside the inner game filesystem.
                    foreach (var meta in pkg.Metas.Metas) {
                        if (meta.Encrypted || !EntryNames.IdToName.ContainsKey(meta.id) || meta.DataSize == 0) continue;
                        string[] parts = ("sce_sys/" + EntryNames.IdToName[meta.id]).Split('/');
                        var dir = inner.GetURoot();
                        for (int i = 0; i < parts.Length - 1; i++) {
                            var next = dir.Get(parts[i]) as PfsReader.Dir;
                            if (next == null) { next = new PfsReader.Dir { name = parts[i], parent = dir }; dir.children.Add(next); }
                            dir = next;
                        }
                        if (dir.Get(parts[parts.Length - 1]) != null) continue;
                        var entryView = map.CreateViewAccessor(meta.DataOffset, meta.DataSize, MemoryMappedFileAccess.Read);
                        dir.children.Add(new PfsReader.File(new MemoryMappedViewAccessor_(entryView)) {
                            name = parts[parts.Length - 1], parent = dir, size = meta.DataSize, compressed_size = meta.DataSize
                        });
                    }
                    bool ready = false;
                    while (true) {
                        var pipe = new NamedPipeServerStream(args[1], PipeDirection.InOut, 16, PipeTransmissionMode.Byte);
                        if (!ready) { Console.WriteLine("READY"); Console.Out.Flush(); ready = true; }
                        pipe.WaitForConnection();
                        ThreadPool.QueueUserWorkItem(delegate { Serve(pipe); });
                    }
                }
            }
        } catch (Exception e) { Console.Error.WriteLine("PKG: " + e.Message); return 1; }
    }
    static void Serve(NamedPipeServerStream pipe) {
        using (pipe) {
            var input = new BinaryReader(pipe, Encoding.UTF8);
            var output = new BinaryWriter(pipe, Encoding.UTF8);
            try {
                while (pipe.IsConnected) {
                    uint op = input.ReadUInt32(), pathLength = input.ReadUInt32();
                    long offset = input.ReadInt64(); uint count = input.ReadUInt32();
                    if (pathLength > 4096 || count > 8 * 1024 * 1024 || offset < 0) break;
                    byte[] pathBytes = input.ReadBytes((int)pathLength);
                    if (pathBytes.Length != pathLength) break;
                    string path = Encoding.UTF8.GetString(pathBytes);
                    if (path.IndexOf('\0') >= 0 || path.IndexOf('\\') >= 0 || path.StartsWith("/") || Array.IndexOf(path.Split('/'), "..") >= 0) {
                        output.Write(13); output.Write(0); output.Flush(); continue;
                    }
                    int error = 0; byte[] reply = new byte[0];
                    try {
                        lock (gate) {
                            var node = path.Length == 0 ? inner.GetURoot() : inner.GetURoot().GetPath(path);
                            if (node == null) error = 2;
                            else using (var result = new MemoryStream()) {
                                var writer = new BinaryWriter(result, Encoding.UTF8);
                                if (op == 1) { writer.Write(node is PfsReader.Dir ? 4 : 8); writer.Write(node is PfsReader.File && node.size != node.compressed_size ? node.compressed_size : node.size); }
                                else if (op == 2) {
                                    var file = node as PfsReader.File;
                                    if (file == null) error = 21;
                                    else {
                                        var size = file.size == file.compressed_size ? file.size : file.compressed_size;
                                        int length = (int)Math.Min(count, Math.Max(0, size - offset));
                                        reply = new byte[length];
                                        var reader = file.GetView();
                                        if (file.size != file.compressed_size) reader = new PFSCReader(reader);
                                        reader.Read(offset, reply, 0, length);
                                    }
                                } else if (op == 3) {
                                    var dir = node as PfsReader.Dir;
                                    if (dir == null) error = 20;
                                    else {
                                        writer.Write(dir.children.Count);
                                        foreach (var child in dir.children) {
                                            var name = Encoding.UTF8.GetBytes(child.name);
                                            writer.Write(child is PfsReader.Dir ? 4 : 8); writer.Write(name.Length); writer.Write(name);
                                        }
                                    }
                                } else error = 22;
                                if (op != 2) reply = result.ToArray();
                            }
                        }
                    } catch (Exception) { error = 5; reply = new byte[0]; }
                    output.Write(error); output.Write(reply.Length); output.Write(reply); output.Flush();
                }
            } catch (IOException) { }
        }
    }
}
