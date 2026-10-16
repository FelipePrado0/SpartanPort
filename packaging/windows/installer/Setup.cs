// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Gow3Setup
{
    internal sealed class SetupPaths
    {
        public string Logs, Settings, Menu, Desktop;
        public static SetupPaths CurrentUser()
        {
            return new SetupPaths {
                Logs = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "gow3-setup", "logs"),
                Settings = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "gow3-launcher", "settings.json"),
                Menu = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "God of War III (PC port)"),
                Desktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory)
            };
        }
    }

    internal sealed class SetupForm : Form
    {
        internal readonly TextBox destination = new TextBox(), game = new TextBox();
        readonly Button browseDestination = new Button(), browseGame = new Button(), install = new Button(), cancel = new Button();
        internal readonly CheckBox desktop = new CheckBox(), open = new CheckBox();
        readonly ProgressBar progress = new ProgressBar();
        readonly Label status = new Label();
        readonly TextBox details = new TextBox();
        readonly string logFile;
        readonly SetupPaths paths;
        CancellationTokenSource cancellation;
        string installed;
        bool busy;
        internal bool IsInstalled { get { return installed != null; } }

        public SetupForm() : this(SetupPaths.CurrentUser()) { }

        internal SetupForm(SetupPaths paths)
        {
            this.paths = paths;
            Text = Texts.Get("God of War III · Setup", "God of War III · Instalador");
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(730, 650);
            MinimumSize = new Size(746, 689);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            AutoScaleMode = AutoScaleMode.Dpi;
            Font = new Font("Segoe UI", 10);
            BackColor = Color.FromArgb(23, 25, 30);
            ForeColor = Color.FromArgb(235, 236, 239);
            Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
            string logs = paths.Logs;
            Directory.CreateDirectory(logs);
            logFile = Path.Combine(logs, DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N") + ".log");
            LabelAt("GOD OF WAR III", 28, 22, 670, 42, 24);
            LabelAt(Texts.Get("Install the PC port in a few clicks.", "Instale o port para PC em poucos cliques."), 30, 72, 665, 28, 11);
            LabelAt(Texts.Get("Downloads the latest official Windows release, including its dependencies.\nYour own PS4 game dump is required and is not downloaded by this installer.",
                "Baixa a versão oficial mais recente para Windows com suas dependências.\nÉ preciso ter seu próprio dump do jogo de PS4, fornecido separadamente."), 30, 111, 670, 54, 10);

            LabelAt(Texts.Get("Install to", "Pasta de instalação"), 30, 180, 650, 25, 10);
            Field(destination, 30, 210, 545);
            destination.Text = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "gow3");
            ButtonAt(browseDestination, Texts.Get("Browse…", "Escolher…"), 585, 207, 115);
            browseDestination.Click += delegate { Choose(destination, false); };

            LabelAt(Texts.Get("Your game folder (optional; select it now or later in the launcher)", "Pasta do seu jogo (opcional; selecione agora ou depois no launcher)"), 30, 255, 670, 25, 10);
            Field(game, 30, 285, 545);
            ButtonAt(browseGame, Texts.Get("Browse…", "Escolher…"), 585, 282, 115);
            browseGame.Click += delegate { Choose(game, true); };
            desktop.Text = Texts.Get("Create a desktop shortcut", "Criar atalho na área de trabalho");
            desktop.Checked = true;
            desktop.SetBounds(30, 333, 660, 28);
            Controls.Add(desktop);
            open.Text = Texts.Get("Open the launcher when finished", "Abrir o launcher ao concluir");
            open.Checked = true;
            open.SetBounds(30, 365, 660, 28);
            Controls.Add(open);

            progress.SetBounds(30, 410, 670, 17);
            Controls.Add(progress);
            status.SetBounds(30, 438, 670, 45);
            status.Text = Texts.Get("Ready. Choose an empty installation folder.", "Pronto. Escolha uma pasta de instalação vazia.");
            Controls.Add(status);
            details.SetBounds(30, 487, 670, 90);
            details.ReadOnly = true;
            details.Multiline = true;
            details.ScrollBars = ScrollBars.Vertical;
            details.BackColor = Color.FromArgb(32, 35, 41);
            details.ForeColor = ForeColor;
            Controls.Add(details);
            ButtonAt(cancel, Texts.Get("Close", "Fechar"), 440, 597, 120);
            cancel.Click += delegate { if (busy) cancellation.Cancel(); else Close(); };
            ButtonAt(install, Texts.Get("Install now", "Instalar agora"), 570, 597, 130);
            install.BackColor = Color.FromArgb(163, 37, 39);
            install.Click += async delegate { if (installed != null) Launch(); else await Install(); };
            FormClosing += delegate(object sender, FormClosingEventArgs e)
            {
                if (busy) { e.Cancel = true; cancellation.Cancel(); }
            };
        }

        void LabelAt(string text, int x, int y, int w, int h, float size)
        {
            var label = new Label { Text = text, Font = new Font("Segoe UI", size), AutoEllipsis = true };
            label.SetBounds(x, y, w, h);
            Controls.Add(label);
        }
        void Field(TextBox box, int x, int y, int width)
        {
            box.SetBounds(x, y, width, 30);
            box.BackColor = Color.FromArgb(40, 43, 50);
            box.ForeColor = ForeColor;
            Controls.Add(box);
        }
        void ButtonAt(Button button, string text, int x, int y, int width)
        {
            button.Text = text;
            button.SetBounds(x, y, width, 34);
            button.FlatStyle = FlatStyle.Flat;
            button.BackColor = Color.FromArgb(49, 53, 61);
            button.ForeColor = ForeColor;
            Controls.Add(button);
        }
        void Choose(TextBox field, bool gameFolder)
        {
            using (var dialog = new FolderBrowserDialog { Description = gameFolder ?
                Texts.Get("Select the folder containing eboot.bin", "Selecione a pasta que contém eboot.bin") :
                Texts.Get("Select an empty folder for the port", "Selecione uma pasta vazia para o port"), SelectedPath = field.Text })
                if (dialog.ShowDialog(this) == DialogResult.OK) field.Text = dialog.SelectedPath;
        }
        void Log(string message)
        {
            string line = DateTime.Now.ToString("HH:mm:ss") + " " + message + Environment.NewLine;
            details.AppendText(line);
            try { File.AppendAllText(logFile, line); }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
        void EnableFields(bool enabled)
        {
            destination.Enabled = game.Enabled = browseDestination.Enabled = browseGame.Enabled =
                desktop.Enabled = open.Enabled = enabled;
        }
        internal async Task Install()
        {
            string target, gamePath = game.Text.Trim();
            try
            {
                InstallerCore.CheckSystem();
                target = InstallerCore.ValidateDestination(destination.Text.Trim());
                InstallerCore.ValidateGame(gamePath);
            }
            catch (Exception error) { status.Text = error.Message; Log(error.Message); return; }
            busy = true;
            cancellation = new CancellationTokenSource();
            EnableFields(false);
            install.Enabled = false;
            cancel.Text = Texts.Get("Cancel", "Cancelar");
            progress.Value = 0;
            string lastMessage = "";
            var reporter = new Progress<InstallProgress>(p =>
            {
                if (!busy || installed != null) return;
                progress.Value = Math.Max(0, Math.Min(100, p.Percent));
                status.Text = p.Message;
                // Keep download telemetry out of the on-disk log at every chunk.
                string stage = p.Percent < 5 ? "metadata" : p.Percent < 66 ? "download" : p.Percent < 70 ? "verify" : "extract";
                if (stage != lastMessage) { Log(p.Message); lastMessage = stage; }
            });
            try
            {
                Release release = await Task.Run(() => InstallerCore.Install(target, cancellation.Token, p => ((IProgress<InstallProgress>)reporter).Report(p)));
                // Installation is committed: cancellation no longer undoes or removes installed files.
                installed = target;
                cancel.Enabled = false;
                Log(release.Version + " · SHA-256 " + release.Sha256);
                Log(target);
                if (!String.IsNullOrEmpty(release.CleanupWarning)) Log(release.CleanupWarning);
                try
                {
                    InstallerCore.ConfigureGame(paths.Settings, gamePath);
                }
                catch (Exception error) { Log(Texts.Get("Select the game folder in the launcher: ", "Selecione a pasta do jogo no launcher: ") + error.Message); }
                try
                {
                    var menu = paths.Menu;
                    InstallerCore.Shortcut(Path.Combine(menu, "God of War III.lnk"), target, false);
                    InstallerCore.Shortcut(Path.Combine(menu, Texts.Get("Play God of War III.lnk", "Jogar God of War III.lnk")), target, true);
                    if (desktop.Checked) InstallerCore.Shortcut(Path.Combine(paths.Desktop, "God of War III.lnk"), target, false);
                }
                catch (Exception error) { Log(Texts.Get("Could not create a shortcut: ", "Não foi possível criar um atalho: ") + error.Message); }
                progress.Value = 100;
                status.Text = Texts.Get("Installed! The launcher will check your game files and graphics hardware.", "Instalado! O launcher fará a verificação do jogo e do hardware gráfico.");
                install.Text = Texts.Get("Open launcher", "Abrir launcher");
                if (open.Checked) Launch();
            }
            catch (OperationCanceledException) { status.Text = Texts.Get("Cancelled. You can start again.", "Cancelado. Você pode iniciar novamente."); Log(status.Text); }
            catch (Exception error)
            {
                status.Text = Texts.Get("Installation failed. See the details below and try again.", "A instalação falhou. Veja os detalhes abaixo e tente novamente.");
                Log(error.ToString());
                Log(Texts.Get("Log: ", "Registro: ") + logFile);
            }
            finally
            {
                busy = false;
                cancellation.Dispose();
                cancellation = null;
                install.Enabled = cancel.Enabled = true;
                cancel.Text = Texts.Get("Close", "Fechar");
                if (installed == null) EnableFields(true);
            }
        }
        void Launch()
        {
            try { Process.Start(new ProcessStartInfo(Path.Combine(installed, "God of War III.exe")) { WorkingDirectory = installed, UseShellExecute = true }); }
            catch (Exception error) { Log(error.Message); }
        }
    }

    internal static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            if (Array.IndexOf(args, "--pt-BR") >= 0) Texts.Portuguese = true;
            if (Array.IndexOf(args, "--en") >= 0) Texts.Portuguese = false;
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            try { Application.Run(new SetupForm()); }
            catch (Exception error) { MessageBox.Show(error.Message, "God of War III Setup", MessageBoxButtons.OK, MessageBoxIcon.Error); }
        }
    }
}
