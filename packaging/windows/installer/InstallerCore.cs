// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace Gow3Setup
{
    internal static class Texts
    {
        public static bool Portuguese = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "pt";
        public static string Get(string en, string pt) { return Portuguese ? pt : en; }
    }

    internal sealed class InstallProgress
    {
        public int Percent;
        public string Message;
        public InstallProgress(int percent, string message) { Percent = percent; Message = message; }
    }

    internal sealed class Release
    {
        public string Version, Url, Sha256;
        public string CleanupWarning;
        public long Size;
    }

    internal static class InstallerCore
    {
        internal const string ReleasesApi = "https://api.github.com/repos/FelipePrado0/gow3_pc/releases/latest";
        internal const long MaxDownload = 512L * 1024 * 1024;
        internal const long MaxExtracted = 2L * 1024 * 1024 * 1024;
        internal static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024 };
        static readonly string[] Required = { "God of War III.exe", "Play God of War III.exe", "bin/gow3-probe.exe", "bin/gow3-gpu-capabilities.exe", "LICENSE" };

        [DllImport("kernel32.dll")]
        static extern bool IsProcessorFeaturePresent(uint feature);

        public static void CheckSystem()
        {
            int build;
            using (var key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Microsoft\Windows NT\CurrentVersion"))
                Int32.TryParse(Convert.ToString(key == null ? null : key.GetValue("CurrentBuildNumber")), out build);
            if (!Environment.Is64BitOperatingSystem || IntPtr.Size != 8 || build < 18362 ||
                !String.Equals(Environment.GetEnvironmentVariable("PROCESSOR_ARCHITECTURE"), "AMD64", StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException(Texts.Get("Windows 10 1903 or newer (x64) is required.", "É necessário Windows 10 1903 ou mais recente (x64)."));
            // PF_AVX_INSTRUCTIONS_AVAILABLE also accounts for OS support for AVX state.
            if (!IsProcessorFeaturePresent(39))
                throw new InvalidOperationException(Texts.Get("This processor/Windows configuration does not support AVX.", "Este processador/configuração do Windows não oferece suporte a AVX."));
        }

        public static string ValidateDestination(string path)
        {
            if (String.IsNullOrWhiteSpace(path) || !Regex.IsMatch(path, @"\A[A-Za-z]:[\\/]"))
                throw new IOException(Texts.Get("Choose an absolute path on a local drive.", "Escolha um caminho completo em um disco local."));
            path = Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar);
            if (path.Length > 170 || path.IndexOf(':', 2) >= 0)
                throw new IOException(Texts.Get("Choose a shorter folder path (up to 170 characters).", "Escolha uma pasta com caminho mais curto (até 170 caracteres)."));
            var root = Path.GetPathRoot(path);
            if (path.TrimEnd('\\').Equals(root.TrimEnd('\\'), StringComparison.OrdinalIgnoreCase))
                throw new IOException(Texts.Get("Choose a new subfolder, not a drive root.", "Escolha uma nova subpasta, em vez da raiz do disco."));
            string[] protectedPaths = { Environment.GetFolderPath(Environment.SpecialFolder.Windows),
                Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
                Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
                Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory) };
            foreach (string protectedPath in protectedPaths.Where(p => !String.IsNullOrEmpty(p)))
            {
                bool system = protectedPath == Environment.GetFolderPath(Environment.SpecialFolder.Windows) ||
                    protectedPath == Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) ||
                    protectedPath == Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
                if (path.Equals(protectedPath, StringComparison.OrdinalIgnoreCase) ||
                    (system && path.StartsWith(protectedPath + "\\", StringComparison.OrdinalIgnoreCase)))
                    throw new IOException(Texts.Get("Choose a new folder in your user profile or another drive.", "Escolha uma nova pasta no seu perfil de usuário ou em outro disco."));
            }
            for (var current = new DirectoryInfo(path); current != null; current = current.Parent)
            {
                if (current.Name.EndsWith(".") || current.Name.EndsWith(" "))
                    throw new IOException("Invalid folder name: " + current.Name);
                if (current.Exists && (current.Attributes & FileAttributes.ReparsePoint) != 0)
                    throw new IOException(Texts.Get("Symbolic links/junctions are not supported as installation paths.", "Links simbólicos/junções não são aceitos no caminho de instalação."));
            }
            if (File.Exists(path) || (Directory.Exists(path) && Directory.EnumerateFileSystemEntries(path).Any()))
                throw new IOException(Texts.Get("Choose an empty folder. To update an existing installation, use Advanced > Check for updates in its launcher.", "Escolha uma pasta vazia. Para atualizar uma instalação existente, use Avançado > Verificar atualizações no launcher."));
            return path;
        }

        public static void ValidateGame(string path)
        {
            if (String.IsNullOrWhiteSpace(path)) return;
            foreach (var name in new[] { "eboot.bin", "sce_sys/param.sfo", "sce_module/libc.prx", "sce_module/libSceFios2.prx" })
            {
                var file = new FileInfo(Path.Combine(path, name));
                if (!file.Exists || file.Length == 0)
                    throw new IOException(Texts.Get("The game folder is missing a required file: ", "Falta um arquivo obrigatório na pasta do jogo: ") + name);
            }
        }

        internal static Release ParseRelease(string json)
        {
            var data = Json.Deserialize<Dictionary<string, object>>(json);
            if (Convert.ToBoolean(data["draft"]) || Convert.ToBoolean(data["prerelease"]))
                throw new IOException("Expected a stable, published release.");
            var assets = (System.Collections.IEnumerable)data["assets"];
            Release result = null;
            foreach (Dictionary<string, object> asset in assets)
            {
                if (Convert.ToString(asset["name"]) != "gow3-windows.zip") continue;
                if (result != null) throw new IOException("Duplicate Windows release asset.");
                string digest = asset.ContainsKey("digest") ? Convert.ToString(asset["digest"]) : "";
                if (!Regex.IsMatch(digest, @"\Asha256:[a-fA-F0-9]{64}\z"))
                    throw new IOException(Texts.Get("The release has no SHA-256 digest. Please use the official release page.", "A versão não tem hash SHA-256. Consulte a página oficial de releases."));
                var uri = new Uri(Convert.ToString(asset["browser_download_url"]));
                if (uri.Scheme != "https" || uri.Host != "github.com" ||
                    !uri.AbsolutePath.StartsWith("/FelipePrado0/gow3_pc/releases/download/", StringComparison.Ordinal) ||
                    !uri.AbsolutePath.EndsWith("/gow3-windows.zip", StringComparison.Ordinal) || !String.IsNullOrEmpty(uri.UserInfo))
                    throw new IOException("Unexpected release download URL.");
                result = new Release { Version = Convert.ToString(data["tag_name"]), Url = uri.AbsoluteUri,
                    Sha256 = digest.Substring(7), Size = Convert.ToInt64(asset["size"]) };
                if (result.Size <= 0 || result.Size > MaxDownload) throw new IOException("Unexpected release size.");
            }
            if (result == null) throw new IOException(Texts.Get("This release has no Windows ZIP.", "Esta versão não tem um ZIP para Windows."));
            return result;
        }

        internal static bool TrustedDownloadUri(Uri uri)
        {
            if (uri.Scheme != "https" || uri.Port != 443 || !String.IsNullOrEmpty(uri.UserInfo)) return false;
            if (uri.Host == "github.com")
                return uri.AbsolutePath.StartsWith("/FelipePrado0/gow3_pc/releases/download/", StringComparison.Ordinal) &&
                    uri.AbsolutePath.EndsWith("/gow3-windows.zip", StringComparison.Ordinal);
            return uri.Host == "release-assets.githubusercontent.com" || uri.Host == "objects.githubusercontent.com";
        }

        static HttpWebRequest Request(string url)
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.UserAgent = "gow3-windows-setup/0.1";
            request.Accept = "application/vnd.github+json";
            request.Headers["X-GitHub-Api-Version"] = "2022-11-28";
            request.Timeout = 30000;
            request.ReadWriteTimeout = 30000;
            // Check each redirect before sending a request, rather than only checking the final URL.
            request.AllowAutoRedirect = false;
            request.AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate;
            return request;
        }

        public static Release Latest(CancellationToken token)
        {
            var request = Request(ReleasesApi);
            using (token.Register(request.Abort))
            {
                try
                {
                    using (var response = (HttpWebResponse)request.GetResponse())
                    using (var stream = response.GetResponseStream())
                    using (var output = new MemoryStream())
                    {
                        if (response.StatusCode != HttpStatusCode.OK)
                            throw new IOException("The official release metadata endpoint returned an unexpected redirect/status.");
                        Copy(stream, output, 4 * 1024 * 1024, token, null);
                        return ParseRelease(Encoding.UTF8.GetString(output.ToArray()));
                    }
                }
                catch (WebException) { token.ThrowIfCancellationRequested(); throw; }
                catch (IOException) { token.ThrowIfCancellationRequested(); throw; }
                catch (ObjectDisposedException) { token.ThrowIfCancellationRequested(); throw; }
            }
        }

        static long Copy(Stream input, Stream output, long limit, CancellationToken token, Action<long> progress)
        {
            var buffer = new byte[128 * 1024];
            long total = 0;
            int count;
            while (true)
            {
                token.ThrowIfCancellationRequested();
                count = input.Read(buffer, 0, buffer.Length);
                if (count == 0) break;
                total += count;
                if (total > limit) throw new IOException("Download/archive exceeds its expected size.");
                output.Write(buffer, 0, count);
                if (progress != null) progress(total);
            }
            return total;
        }

        static HttpWebResponse DownloadResponse(string url, CancellationToken token, out HttpWebRequest activeRequest)
        {
            Uri uri = new Uri(url);
            for (int redirect = 0; redirect <= 5; redirect++)
            {
                token.ThrowIfCancellationRequested();
                if (!TrustedDownloadUri(uri)) throw new IOException("Untrusted release download redirect.");
                activeRequest = Request(uri.AbsoluteUri);
                activeRequest.Accept = "application/octet-stream";
                activeRequest.Headers.Remove("X-GitHub-Api-Version");
                HttpWebResponse response;
                using (token.Register(activeRequest.Abort))
                    response = (HttpWebResponse)activeRequest.GetResponse();
                int status = (int)response.StatusCode;
                if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
                {
                    string location = response.Headers["Location"];
                    response.Dispose();
                    if (String.IsNullOrEmpty(location)) throw new IOException("Missing redirect location.");
                    uri = new Uri(uri, location);
                    continue;
                }
                if (response.StatusCode != HttpStatusCode.OK)
                {
                    response.Dispose();
                    throw new IOException("Unexpected release download status.");
                }
                return response;
            }
            throw new IOException("Too many release download redirects.");
        }

        internal static void Download(Release release, string file, CancellationToken token, Action<InstallProgress> progress)
        {
            try
            {
                HttpWebRequest request;
                using (var response = DownloadResponse(release.Url, token, out request))
                using (token.Register(request.Abort))
                using (var input = response.GetResponseStream())
                using (var output = new FileStream(file, FileMode.CreateNew))
                {
                    var watch = Stopwatch.StartNew();
                    long lastReport = -500;
                    long total = Copy(input, output, release.Size, token, delegate(long bytes)
                    {
                        if (watch.ElapsedMilliseconds - lastReport < 150) return;
                        lastReport = watch.ElapsedMilliseconds;
                        double speed = bytes / Math.Max(0.1, watch.Elapsed.TotalSeconds) / 1048576;
                        progress(new InstallProgress(5 + (int)(bytes * 60 / release.Size),
                            String.Format(Texts.Get("Downloading {0}: {1:F1} / {2:F1} MB · {3:F1} MB/s", "Baixando {0}: {1:F1} / {2:F1} MB · {3:F1} MB/s"),
                                release.Version, bytes / 1048576.0, release.Size / 1048576.0, speed)));
                    });
                    if (total != release.Size) throw new IOException("Incomplete download.");
                }
            }
            catch (WebException) { token.ThrowIfCancellationRequested(); throw; }
            catch (IOException) { token.ThrowIfCancellationRequested(); throw; }
            catch (ObjectDisposedException) { token.ThrowIfCancellationRequested(); throw; }
        }

        internal static void Verify(string file, string expected, CancellationToken token)
        {
            token.ThrowIfCancellationRequested();
            using (var input = File.OpenRead(file))
            using (var hash = SHA256.Create())
            {
                var actual = BitConverter.ToString(hash.ComputeHash(input)).Replace("-", "");
                if (!String.Equals(actual, expected, StringComparison.OrdinalIgnoreCase))
                    throw new IOException(Texts.Get("SHA-256 verification failed. Nothing was installed; try again.", "A verificação SHA-256 falhou. Nada foi instalado; tente novamente."));
            }
            token.ThrowIfCancellationRequested();
        }

        internal static string EntryPath(string entry)
        {
            var parts = entry.Replace('\\', '/').Split('/');
            if (parts.Length < 2 || parts[0] != "gow3-windows") throw new IOException("Unexpected archive root.");
            for (int i = 1; i < parts.Length; i++)
            {
                string part = parts[i];
                if (i == parts.Length - 1 && part == "") continue;
                if (part == "" || part == "." || part == ".." || part.EndsWith(".") || part.EndsWith(" ") ||
                    part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 ||
                    Regex.IsMatch(part, @"\A(CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|CLOCK\$|COM[0-9¹²³]|LPT[0-9¹²³]) *(?:\.|\z)", RegexOptions.IgnoreCase))
                    throw new IOException("Unsafe archive entry: " + entry);
            }
            string top = parts[1].ToLowerInvariant();
            if (new[] { "user", "out", "mods", "gow3.ini", "mods.json", "mods_presets.json", "patches.json", "settings.json", "last_run.log" }.Contains(top))
                throw new IOException("Archive contains user data: " + entry);
            return String.Join(Path.DirectorySeparatorChar.ToString(), parts.Skip(1).ToArray());
        }

        internal static void Extract(string zip, string payload, CancellationToken token, Action<InstallProgress> progress)
        {
            Directory.CreateDirectory(payload);
            using (var archive = ZipFile.OpenRead(zip))
            {
                if (archive.Entries.Count > 50000) throw new IOException("Too many archive entries.");
                var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                long total = 0;
                foreach (var entry in archive.Entries)
                {
                    string relative = EntryPath(entry.FullName);
                    if (!seen.Add(relative.TrimEnd('\\'))) throw new IOException("Duplicate archive entry.");
                    if (((entry.ExternalAttributes >> 16) & 0xF000) == 0xA000 || (entry.ExternalAttributes & 0x400) != 0)
                        throw new IOException("Archive links are not supported.");
                    total = checked(total + entry.Length);
                    if (total > MaxExtracted) throw new IOException("Archive is too large.");
                }
                if (new DriveInfo(Path.GetPathRoot(payload)).AvailableFreeSpace < total + 64L * 1024 * 1024)
                    throw new IOException(Texts.Get("Not enough free space on the installation drive.", "Não há espaço livre suficiente no disco de instalação."));
                long extracted = 0;
                foreach (var entry in archive.Entries)
                {
                    token.ThrowIfCancellationRequested();
                    string relative = EntryPath(entry.FullName);
                    if (relative == "") continue;
                    string target = Path.Combine(payload, relative);
                    if (entry.FullName.EndsWith("/") || entry.FullName.EndsWith("\\"))
                    { Directory.CreateDirectory(target); continue; }
                    Directory.CreateDirectory(Path.GetDirectoryName(target));
                    using (var input = entry.Open())
                    using (var output = new FileStream(target, FileMode.CreateNew))
                    {
                        long copied = Copy(input, output, entry.Length, token, null);
                        if (copied != entry.Length) throw new IOException("Incomplete archive entry.");
                    }
                    extracted += entry.Length;
                    progress(new InstallProgress(70 + (int)(extracted * 25 / Math.Max(1, total)),
                        Texts.Get("Extracting files…", "Extraindo arquivos…")));
                }
            }
            foreach (string required in Required)
                if (!File.Exists(Path.Combine(payload, required)) || new FileInfo(Path.Combine(payload, required)).Length == 0)
                    throw new IOException("Incomplete Windows package: " + required);
        }

        internal static string CreateWork(string destination)
        {
            string parent = Path.GetDirectoryName(ValidateDestination(destination));
            Directory.CreateDirectory(parent);
            var work = Path.Combine(parent, ".gow3-setup-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(work);
            return work;
        }

        // Only accepts a direct child we created; never follows reparse points during cleanup.
        internal static void Cleanup(string work, string destination)
        {
            string parent = Path.GetDirectoryName(Path.GetFullPath(destination));
            string full = Path.GetFullPath(work);
            if (!String.Equals(Path.GetDirectoryName(full), parent, StringComparison.OrdinalIgnoreCase) ||
                !Regex.IsMatch(Path.GetFileName(full), @"\A\.gow3-setup-[a-f0-9]{32}\z"))
                throw new IOException("Refusing to clean an unowned path.");
            if (Directory.Exists(full)) DeleteTree(new DirectoryInfo(full));
        }

        static void DeleteTree(DirectoryInfo directory)
        {
            if ((directory.Attributes & FileAttributes.ReparsePoint) == 0)
            {
                foreach (var child in directory.GetDirectories()) DeleteTree(child);
                foreach (var file in directory.GetFiles()) file.Delete();
            }
            directory.Delete();
        }

        internal static void Commit(string zip, string destination, string sha256, string work,
            CancellationToken token, Action<InstallProgress> progress)
        {
            progress(new InstallProgress(66, Texts.Get("Verifying SHA-256…", "Conferindo SHA-256…")));
            Verify(zip, sha256, token);
            string payload = Path.Combine(work, "payload");
            Extract(zip, payload, token, progress);
            // Recheck after downloading/extracting; another process may have used the destination.
            ValidateDestination(destination);
            token.ThrowIfCancellationRequested();
            if (Directory.Exists(destination)) Directory.Delete(destination, false);
            Directory.Move(payload, destination);
        }

        public static Release Install(string destination, CancellationToken token, Action<InstallProgress> progress)
        {
            CheckSystem();
            destination = ValidateDestination(destination);
            progress(new InstallProgress(1, Texts.Get("Looking up the latest official release…", "Consultando a versão oficial mais recente…")));
            Release release = Latest(token);
            string work = CreateWork(destination);
            bool committed = false;
            try
            {
                if (new DriveInfo(Path.GetPathRoot(destination)).AvailableFreeSpace < release.Size + 256L * 1024 * 1024)
                    throw new IOException(Texts.Get("Not enough free disk space.", "Não há espaço livre suficiente no disco."));
                string zip = Path.Combine(work, "download.zip");
                Download(release, zip, token, progress);
                Commit(zip, destination, release.Sha256, work, token, progress);
                committed = true;
                return release;
            }
            finally { release.CleanupWarning = FinishCleanup(work, destination, committed); }
        }

        internal static string FinishCleanup(string work, string destination, bool committed)
        {
            try { Cleanup(work, destination); }
            catch (IOException error)
            {
                if (!committed) throw;
                return Texts.Get("Installed, but temporary files could not be removed: ", "Instalado, mas não foi possível remover arquivos temporários: ") + work + " · " + error.Message;
            }
            catch (UnauthorizedAccessException error)
            {
                if (!committed) throw;
                return Texts.Get("Installed, but temporary files could not be removed: ", "Instalado, mas não foi possível remover arquivos temporários: ") + work + " · " + error.Message;
            }
            return null;
        }

        internal static void ConfigureGame(string settingsFile, string game)
        {
            if (String.IsNullOrWhiteSpace(game)) return;
            ValidateGame(game);
            var settings = File.Exists(settingsFile) ? Json.Deserialize<Dictionary<string, object>>(File.ReadAllText(settingsFile)) : new Dictionary<string, object>();
            settings["game_dir"] = Path.GetFullPath(game);
            if (!settings.ContainsKey("ui_language")) settings["ui_language"] = Texts.Portuguese ? "pt" : "en";
            Directory.CreateDirectory(Path.GetDirectoryName(settingsFile));
            string temp = settingsFile + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                File.WriteAllText(temp, Json.Serialize(settings), new UTF8Encoding(false));
                if (File.Exists(settingsFile)) File.Replace(temp, settingsFile, settingsFile + ".setup-backup-" + Guid.NewGuid().ToString("N"));
                else File.Move(temp, settingsFile);
            }
            finally { if (File.Exists(temp)) File.Delete(temp); }
        }

        internal static string Shortcut(string link, string destination, bool play)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(link));
            // Keep shortcuts from previous installations or other launchers.
            string requested = link;
            for (int suffix = 2; File.Exists(link) || Directory.Exists(link); suffix++)
                link = Path.Combine(Path.GetDirectoryName(requested), Path.GetFileNameWithoutExtension(requested) + " (" + suffix + ").lnk");
            object shell = null, shortcut = null;
            try
            {
                var type = Type.GetTypeFromProgID("WScript.Shell", true);
                shell = Activator.CreateInstance(type);
                shortcut = type.InvokeMember("CreateShortcut", System.Reflection.BindingFlags.InvokeMethod, null, shell, new object[] { link });
                var shortcutType = shortcut.GetType();
                Action<string, object> set = (name, value) => shortcutType.InvokeMember(name, System.Reflection.BindingFlags.SetProperty, null, shortcut, new object[] { value });
                set("TargetPath", Path.Combine(destination, play ? "Play God of War III.exe" : "God of War III.exe"));
                set("WorkingDirectory", destination);
                set("IconLocation", Path.Combine(destination, "launcher", "gow3.ico"));
                shortcutType.InvokeMember("Save", System.Reflection.BindingFlags.InvokeMethod, null, shortcut, null);
                return link;
            }
            finally
            {
                if (shortcut != null) Marshal.FinalReleaseComObject(shortcut);
                if (shell != null) Marshal.FinalReleaseComObject(shell);
            }
        }
    }
}
