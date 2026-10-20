SpartanPort: experimental Apple Silicon build

Requirements: Apple Silicon (M1 or newer), macOS 26 or newer and Rosetta 2.
Intel Macs are not supported by this package. Python and Homebrew are not required.
Install Rosetta using Apple's prompt when opening the application, if requested.

Open spartanport-macos.dmg and drag SpartanPort.app into Applications.
Start SpartanPort and select your own compatible game dump in the launcher.
This download contains no game files, saves or shader caches.

This test application has an ad hoc signature, not Apple Developer ID notarization.
If macOS blocks it, verify the download source and checksum first. The test build may
require removing its quarantine attribute locally before it can open:
  xattr -dr com.apple.quarantine /Applications/SpartanPort.app
Do not disable Gatekeeper globally.

Saves, settings, logs and generated files are kept outside the application:
  ~/Library/Application Support/SpartanPort
Replacing the application preserves that folder.

For the first test, use 720p and a 60 FPS engine rate and frame limit.
The menu opens with Insert, the key left of 1 (`), or the controller shortcut.
Report the Mac model, RAM, macOS version, game build, resolution and the scene tested.
Use the launcher's log export when reporting a startup or gameplay problem.

Experimental means gameplay and compatibility may vary. Build success alone is not
confirmation of gameplay, controller support or completion of the game.

Contributions consolidated in PR #798:
  Fleurat (#798): Apple Silicon runtime, KosmicKrisp and renderer optimizations.
  nizwar (#510): application packaging and bundle/user-data separation.
  Incognito-Coder (#469): macOS build automation and CoreAudio queue improvements.
Upstream credits: Supermedo/bloodborne_pc, deadinside28/bloodborne_pc and shadPS4.
See the included licenses and VENDOR.txt for third-party components.
