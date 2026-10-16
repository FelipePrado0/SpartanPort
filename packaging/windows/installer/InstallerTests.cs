// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone tests: the Windows compiler is the only build dependency.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Gow3Setup
{
    internal static class InstallerTests
    {
        static int passed;
        static string root;
        static readonly Action<InstallProgress> Quiet = p => { };
        static void Assert(bool value, string message) { if (!value) throw new Exception(message); }
        static void Reject(Action action)
        {
            try { action(); }
            catch (IOException) { return; }
            throw new Exception("Expected an IOException.");
        }
        static void Test(string name, Action action)
        {
            action(); passed++;
            Console.WriteLine("PASS " + name);
        }
        static string Digest(string path)
        {
            using (var hash = SHA256.Create())
            using (var stream = File.OpenRead(path)) return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-", "");
        }
        static string Archive(string name, params string[] entries)
        {
            string file = Path.Combine(root, name + ".zip");
            using (var archive = ZipFile.Open(file, ZipArchiveMode.Create))
                foreach (string entry in entries)
                    using (var writer = new StreamWriter(archive.CreateEntry(entry).Open())) writer.Write("fixture");
            return file;
        }
        static string ValidArchive(string name)
        {
            return Archive(name, "gow3-windows/God of War III.exe", "gow3-windows/Play God of War III.exe",
                "gow3-windows/bin/gow3-probe.exe", "gow3-windows/bin/gow3-gpu-capabilities.exe", "gow3-windows/LICENSE");
        }
        static void WithWork(string destination, Action<string> action)
        {
            string work = InstallerCore.CreateWork(destination);
            try { action(work); }
            finally { InstallerCore.Cleanup(work, destination); }
        }
        static string ReleaseJson(string digest, string url, bool pre)
        {
            return InstallerCore.Json.Serialize(new { tag_name = "windows-v1.0.0", draft = false, prerelease = pre,
                assets = new[] { new { name = "gow3-windows.zip", size = 100, digest = digest, browser_download_url = url } } });
        }
        [STAThread]
        static int Main(string[] args)
        {
            root = Path.Combine(Path.GetTempPath(), "gow3-installer-tests-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                Run();
                if (args.Contains("--integration")) Integration();
                Console.WriteLine(passed + " tests passed.");
                return 0;
            }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
            finally
            {
                // root is a fresh GUID directory directly below the test temp directory.
                if (Path.GetDirectoryName(root) != Path.GetTempPath().TrimEnd('\\') ||
                    !Path.GetFileName(root).StartsWith("gow3-installer-tests-")) throw new Exception("Unsafe test cleanup path.");
                Directory.Delete(root, true);
            }
        }
        static void Run()
        {
            string url = "https://github.com/FelipePrado0/gow3_pc/releases/download/windows-v1.0.0/gow3-windows.zip";
            string digest = "sha256:" + new string('a', 64);
            Test("select official Windows asset", () => Assert(InstallerCore.ParseRelease(ReleaseJson(digest, url, false)).Sha256 == new string('a', 64), "digest"));
            Test("reject missing digest", () => Reject(() => InstallerCore.ParseRelease(ReleaseJson(null, url, false))));
            Test("reject another repository", () => Reject(() => InstallerCore.ParseRelease(ReleaseJson(digest, url.Replace("FelipePrado0", "other"), false))));
            Test("reject HTTP", () => Reject(() => InstallerCore.ParseRelease(ReleaseJson(digest, url.Replace("https:", "http:"), false))));
            Test("reject prerelease", () => Reject(() => InstallerCore.ParseRelease(ReleaseJson(digest, url, true))));
            Test("reject relative destination", () => Reject(() => InstallerCore.ValidateDestination("relative")));
            Test("reject drive-relative destination", () => Reject(() => InstallerCore.ValidateDestination("C:relative")));
            Test("reject drive root", () => Reject(() => InstallerCore.ValidateDestination(Path.GetPathRoot(root))));
            Test("reject user profile root", () => Reject(() => InstallerCore.ValidateDestination(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile))));
            Test("reject Windows directory", () => Reject(() => InstallerCore.ValidateDestination(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "gow3"))));
            Test("reject occupied destination", () => {
                string occupied = Path.Combine(root, "occupied"); Directory.CreateDirectory(occupied);
                File.WriteAllText(Path.Combine(occupied, "save.dat"), "keep");
                Reject(() => InstallerCore.ValidateDestination(occupied));
                Assert(File.ReadAllText(Path.Combine(occupied, "save.dat")) == "keep", "user file changed");
            });
            Test("reject unsafe archive names", () => {
                foreach (string name in new[] { "gow3-windows/../outside", "gow3-windows/dir/../../outside", "gow3-windows/C:/outside",
                    "gow3-windows/bin/file:stream", "gow3-windows/CON.txt", "gow3-windows/CON .txt", "gow3-windows/COM²", "gow3-windows/CONOUT$",
                    "gow3-windows/file. ", "gow3-windows//bad",
                    "/absolute", "other/file", "gow3-windows/user/save", "gow3-windows/gow3.ini", @"gow3-windows\..\outside" })
                    Reject(() => InstallerCore.EntryPath(name));
            });
            Test("valid extraction and atomic install", () => {
                string zip = ValidArchive("valid"), destination = Path.Combine(root, "instalação com espaços");
                WithWork(destination, work => InstallerCore.Commit(zip, destination, Digest(zip), work, CancellationToken.None, Quiet));
                Assert(File.ReadAllText(Path.Combine(destination, "God of War III.exe")) == "fixture", "missing payload");
                Assert(!Directory.GetDirectories(root, ".gow3-setup-*").Any(), "staging left behind");
            });
            Test("bad digest leaves destination untouched", () => {
                string zip = ValidArchive("badhash"), destination = Path.Combine(root, "badhash");
                WithWork(destination, work => Reject(() => InstallerCore.Commit(zip, destination, new string('0', 64), work, CancellationToken.None, Quiet)));
                Assert(!Directory.Exists(destination), "destination created before verification");
            });
            Test("reject duplicate files ignoring case", () => {
                string zip = Archive("duplicates", "gow3-windows/bin/file", "gow3-windows/bin/FILE");
                WithWork(Path.Combine(root, "duplicates"), work => Reject(() => InstallerCore.Extract(zip, Path.Combine(work, "payload"), CancellationToken.None, Quiet)));
            });
            Test("reject traversal before extraction", () => {
                string zip = Archive("traversal", "gow3-windows/../escaped.txt");
                WithWork(Path.Combine(root, "traversal"), work => Reject(() => InstallerCore.Extract(zip, Path.Combine(work, "payload"), CancellationToken.None, Quiet)));
                Assert(!File.Exists(Path.Combine(root, "escaped.txt")), "zip escaped staging");
            });
            Test("reject archive symlink", () => {
                string zip = Archive("symlink", "gow3-windows/link");
                using (var archive = ZipFile.Open(zip, ZipArchiveMode.Update)) archive.Entries[0].ExternalAttributes = unchecked((int)0xA1FF0000);
                WithWork(Path.Combine(root, "symlink"), work => Reject(() => InstallerCore.Extract(zip, Path.Combine(work, "payload"), CancellationToken.None, Quiet)));
            });
            Test("incomplete package cannot commit", () => {
                string zip = Archive("incomplete", "gow3-windows/LICENSE"), destination = Path.Combine(root, "incomplete");
                WithWork(destination, work => Reject(() => InstallerCore.Commit(zip, destination, Digest(zip), work, CancellationToken.None, Quiet)));
                Assert(!Directory.Exists(destination), "incomplete package installed");
            });
            Test("cancel extraction without installing partial files", () => {
                string zip = ValidArchive("cancel"), destination = Path.Combine(root, "cancel");
                using (var cancel = new CancellationTokenSource())
                    WithWork(destination, work => {
                        bool cancelled = false;
                        try { InstallerCore.Commit(zip, destination, Digest(zip), work, cancel.Token, p => { if (p.Percent >= 70) cancel.Cancel(); }); }
                        catch (OperationCanceledException) { cancelled = true; }
                        Assert(cancelled && !Directory.Exists(destination), "cancellation installed files");
                    });
            });
            Test("concurrent destination change is preserved", () => {
                string zip = ValidArchive("race"), destination = Path.Combine(root, "race");
                WithWork(destination, work => Reject(() => InstallerCore.Commit(zip, destination, Digest(zip), work, CancellationToken.None, p => {
                    if (p.Percent >= 70 && !Directory.Exists(destination)) { Directory.CreateDirectory(destination); File.WriteAllText(Path.Combine(destination, "save.dat"), "keep"); }
                })));
                Assert(File.ReadAllText(Path.Combine(destination, "save.dat")) == "keep", "concurrent file changed");
            });
            Test("cleanup refuses an unowned folder", () => Reject(() => InstallerCore.Cleanup(root, Path.Combine(root, "target"))));
            Test("only allow HTTPS GitHub asset redirects", () => {
                foreach (string trusted in new[] { url, "https://release-assets.githubusercontent.com/github-production-release-asset/test?signature=test", "https://objects.githubusercontent.com/test" })
                    Assert(InstallerCore.TrustedDownloadUri(new Uri(trusted)), "trusted endpoint rejected");
                foreach (string untrusted in new[] { url.Replace("https:", "http:"), "https://github.com/other/repo/releases/download/v1/gow3-windows.zip",
                    "https://release-assets.githubusercontent.com.evil.example/test", "https://example.com/test", "https://github.com:8443/FelipePrado0/gow3_pc/releases/download/v1/gow3-windows.zip",
                    "https://user@release-assets.githubusercontent.com/test", "file:///C:/test" })
                    Assert(!InstallerCore.TrustedDownloadUri(new Uri(untrusted)), "untrusted endpoint accepted: " + untrusted);
            });
            Test("failed temporary cleanup does not undo a committed installation", () => {
                string destination = Path.Combine(root, "cleanup-failure");
                WithWork(destination, work => {
                    string locked = Path.Combine(work, "locked.zip");
                    using (var stream = new FileStream(locked, FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None)) {
                        Assert(!String.IsNullOrEmpty(InstallerCore.FinishCleanup(work, destination, true)), "cleanup warning missing");
                        Reject(() => InstallerCore.FinishCleanup(work, destination, false));
                    }
                });
            });
            Test("reject an installation path through a junction", () => {
                string target = Path.Combine(root, "junction-target"), link = Path.Combine(root, "junction-link");
                Directory.CreateDirectory(target);
                CreateJunction(link, target);
                try { Reject(() => InstallerCore.ValidateDestination(Path.Combine(link, "install"))); }
                finally { Directory.Delete(link, false); }
            });
            Test("temporary cleanup never follows a junction", () => {
                string target = Path.Combine(root, "preserve-junction-target");
                Directory.CreateDirectory(target);
                string sentinel = Path.Combine(target, "save.dat");
                File.WriteAllText(sentinel, "keep");
                WithWork(Path.Combine(root, "cleanup-junction"), work => CreateJunction(Path.Combine(work, "link"), target));
                Assert(File.ReadAllText(sentinel) == "keep", "cleanup followed a junction");
            });
            Test("game folder validation rejects incomplete data", () => Reject(() => InstallerCore.ValidateGame(root)));
            Test("optional game folder can be skipped", () => InstallerCore.ValidateGame(""));
            Test("preserve launcher settings and make backup", () => {
                string game = Path.Combine(root, "game"), config = Path.Combine(root, "config", "settings.json");
                foreach (string name in new[] { "eboot.bin", "sce_sys/param.sfo", "sce_module/libc.prx", "sce_module/libSceFios2.prx" }) {
                    Directory.CreateDirectory(Path.GetDirectoryName(Path.Combine(game, name))); File.WriteAllText(Path.Combine(game, name), "fixture");
                }
                Directory.CreateDirectory(Path.GetDirectoryName(config));
                string original = "{\"gpu_id\":\"2\",\"custom\":{\"keep\":true},\"ui_language\":\"es\"}";
                File.WriteAllText(config, original);
                InstallerCore.ConfigureGame(config, game);
                var updated = InstallerCore.Json.Deserialize<Dictionary<string, object>>(File.ReadAllText(config));
                Assert((string)updated["gpu_id"] == "2" && (string)updated["ui_language"] == "es" && (string)updated["game_dir"] == game, "settings not preserved");
                Assert(File.ReadAllText(Directory.GetFiles(Path.GetDirectoryName(config), "*.setup-backup-*").Single()) == original, "backup mismatch");
            });
            Test("create local shortcuts without changing desktop", () => {
                string link = Path.Combine(root, "test.lnk");
                InstallerCore.Shortcut(link, Path.Combine(root, "instalação com espaços"), false);
                Assert(new FileInfo(link).Length > 0, "missing shortcut");
            });
            Test("keep existing shortcuts", () => {
                string link = Path.Combine(root, "existing.lnk");
                File.WriteAllText(link, "keep existing shortcut");
                string created = InstallerCore.Shortcut(link, root, true);
                Assert(File.ReadAllText(link) == "keep existing shortcut", "shortcut overwritten");
                Assert(created != link && File.Exists(created), "new shortcut missing");
            });
            Test("invalid settings JSON is preserved", () => {
                string config = Path.Combine(root, "invalid-settings.json");
                File.WriteAllText(config, "{not json}");
                bool rejected = false;
                try { InstallerCore.ConfigureGame(config, Path.Combine(root, "game")); }
                catch (ArgumentException) { rejected = true; }
                Assert(rejected && File.ReadAllText(config) == "{not json}", "invalid config changed");
            });
            Test("render installer in Portuguese and English", () => {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                bool original = Texts.Portuguese;
                try {
                    foreach (bool portuguese in new[] { true, false }) {
                        Texts.Portuguese = portuguese;
                        using (var form = new SetupForm(TestPaths("preview")))
                        using (var bitmap = new Bitmap(form.Width, form.Height)) {
                            // DrawToBitmap omits child controls until their handles exist.
                            // Show off-screen so the test never opens a visible installer.
                            form.StartPosition = FormStartPosition.Manual;
                            form.Location = new Point(-32000, -32000);
                            form.ShowInTaskbar = false;
                            form.Show();
                            Application.DoEvents();
                            form.DrawToBitmap(bitmap, new Rectangle(Point.Empty, form.Size));
                            bitmap.Save(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, portuguese ? "preview.pt-BR.png" : "preview.en.png"), ImageFormat.Png);
                            form.Close();
                        }
                    }
                }
                finally { Texts.Portuguese = original; }
            });
        }
        static void Integration()
        {
            string destination = Path.Combine(root, "real-install");
            Test("real GUI installation, game-folder configuration and shortcuts", () => {
                var paths = TestPaths("gui-install");
                using (var form = new SetupForm(paths)) {
                    form.StartPosition = FormStartPosition.Manual;
                    form.Location = new Point(-32000, -32000);
                    form.ShowInTaskbar = false;
                    form.Show();
                    form.destination.Text = destination;
                    form.game.Text = Path.Combine(root, "game");
                    form.open.Checked = false;
                    var task = form.Install();
                    var watch = Stopwatch.StartNew();
                    while (!task.IsCompleted && watch.Elapsed.TotalSeconds < 120) {
                        Application.DoEvents();
                        Thread.Sleep(10);
                    }
                    Assert(task.IsCompleted, "GUI installation timed out");
                    task.GetAwaiter().GetResult();
                    if (!form.IsInstalled) {
                        foreach (string log in Directory.GetFiles(paths.Logs, "*.log")) Console.WriteLine(File.ReadAllText(log));
                    }
                    Assert(form.IsInstalled, "GUI did not complete installation");
                    form.Close();
                }
                Assert(new FileInfo(Path.Combine(destination, "God of War III.exe")).Length > 1024, "real executable missing");
                Assert(File.Exists(Path.Combine(paths.Menu, "God of War III.lnk")), "Start menu shortcut missing");
                Assert(File.Exists(Path.Combine(paths.Desktop, "God of War III.lnk")), "desktop shortcut missing");
                var settings = InstallerCore.Json.Deserialize<Dictionary<string, object>>(File.ReadAllText(paths.Settings));
                Assert((string)settings["game_dir"] == Path.Combine(root, "game"), "game folder not configured");
                Assert(!Directory.GetDirectories(root, ".gow3-setup-*").Any(), "staging left behind");
                foreach (string log in Directory.GetFiles(paths.Logs, "*.log")) Console.WriteLine(File.ReadAllText(log));
            });
            Test("packaged Python, Tk, Pillow and iced-x86 start in an isolated profile", () => {
                string script = Path.Combine(root, "runtime-smoke.py");
                File.WriteAllText(script, "import tkinter\nfrom PIL import Image\nimport iced_x86\nroot = tkinter.Tk()\nroot.withdraw()\nroot.update()\nroot.destroy()\nprint('GOW3_SETUP_RUNTIME_OK', flush=True)\n");
                string output = RunHidden(Path.Combine(destination, "God of War III.exe"), "--script \"" + script + "\"", destination);
                Assert(output.Contains("GOW3_SETUP_RUNTIME_OK"), "runtime marker missing: " + output);
            });
            Test("packaged Vulkan GPU probe starts", () => {
                string output = RunHidden(Path.Combine(destination, "bin", "gow3-gpu-capabilities.exe"), "--list", destination);
                Console.WriteLine("Vulkan GPU probe:\n" + output);
                Assert(output.Split('\n').Any(line => line.Length > 1 && Char.IsDigit(line[0]) && line.Contains("\t")), "GPU probe returned no device");
            });
            Test("cancel a real download and remove partial files", () => {
                string cancelledDestination = Path.Combine(root, "real-cancel");
                using (var cancel = new CancellationTokenSource()) {
                    bool cancelled = false;
                    try {
                        InstallerCore.Install(cancelledDestination, cancel.Token, p => { if (p.Percent >= 5) cancel.Cancel(); });
                    }
                    catch (OperationCanceledException) { cancelled = true; }
                    Assert(cancelled && !Directory.Exists(cancelledDestination), "cancelled download installed files");
                    Assert(!Directory.GetDirectories(root, ".gow3-setup-*").Any(), "partial download left behind");
                }
            });
        }

        static SetupPaths TestPaths(string name)
        {
            string folder = Path.Combine(root, name);
            return new SetupPaths { Logs = Path.Combine(folder, "logs"), Settings = Path.Combine(folder, "config", "settings.json"),
                Menu = Path.Combine(folder, "start-menu"), Desktop = Path.Combine(folder, "desktop") };
        }

        static void CreateJunction(string link, string target)
        {
            string command = "$ErrorActionPreference='Stop'; New-Item -ItemType Junction -Path '" + link.Replace("'", "''") + "' -Target '" + target.Replace("'", "''") + "' | Out-Null";
            string encoded = Convert.ToBase64String(System.Text.Encoding.Unicode.GetBytes(command));
            RunHidden(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32", "WindowsPowerShell", "v1.0", "powershell.exe"),
                "-NoProfile -NonInteractive -EncodedCommand " + encoded, root);
        }

        static string RunHidden(string executable, string arguments, string directory)
        {
            var info = new ProcessStartInfo(executable, arguments) { WorkingDirectory = directory, UseShellExecute = false,
                CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden, RedirectStandardOutput = true, RedirectStandardError = true };
            string profile = Path.Combine(root, "runtime-profile");
            Directory.CreateDirectory(profile);
            info.EnvironmentVariables["APPDATA"] = profile;
            info.EnvironmentVariables["LOCALAPPDATA"] = profile;
            info.EnvironmentVariables["GOW3_DATA_DIR"] = directory;
            using (var process = Process.Start(info)) {
                Task<string> stdout = process.StandardOutput.ReadToEndAsync(), stderr = process.StandardError.ReadToEndAsync();
                if (!process.WaitForExit(30000)) { process.Kill(); process.WaitForExit(); throw new Exception("Runtime probe timed out."); }
                string output = stdout.GetAwaiter().GetResult() + stderr.GetAwaiter().GetResult();
                Assert(process.ExitCode == 0, "Runtime probe exited " + process.ExitCode + ": " + output);
                return output;
            }
        }
    }
}
