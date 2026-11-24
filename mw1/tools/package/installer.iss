; MechWarrior 1 Remake's Windows installer for players: Inno Setup (jrsoftware.org, free), built by
; mw1\tools\package\package.cmd from the package folder it has just zipped, so the Setup and the zip hold the same
; files:
;   iscc /DAppVersion=<mw1-vX.Y.Z> /DDist=<the package folder> /O<out dir> mw1\tools\package\installer.iss
; Out: MW1-Remake-Setup-<mw1-vX.Y.Z>.exe, beside MW1-Remake-<mw1-vX.Y.Z>-win64.zip (the zip stays: the launcher
; installs MW1 from it, never from the Setup).
; It installs to Program Files\MechWarrior 1 Remake with a Start-menu shortcut (a desktop one ticked by default) and
; an uninstaller. Its own AppId, folder, shortcut names and uninstall entry, none shared with MechWarrior 3
; Rebuild's or Pirate's Moon Rebuild's installers, so the three install, upgrade and uninstall apart. The game writes
; nothing in its folder: the player's settings, saves and the paths to the games live in
; %APPDATA%\BattleTech\MechWarrior 1 Remake (MicroProse's before 1.1.0), which the uninstaller leaves alone. No file of any game is in the
; installer. PM's own tools\windows\installer.iss is left as it is, so merges from mech3-pm-re never meet this file.

#ifndef AppVersion
  #define AppVersion "dev"
#endif
#ifndef Dist
  #error Give /DDist=<the package folder> (package.cmd does).
#endif

[Setup]
; The same AppId every version: a new installer upgrades the old install in place. MW1's own (MW3's and PM's are
; their installers' own).
AppId={{C33F5AC5-1EFA-4952-81EA-A76A9A48AF28}
AppName=MechWarrior 1 Remake
AppVersion={#AppVersion}
AppVerName=MechWarrior 1 Remake {#AppVersion}
AppPublisher=MechWarrior 1 Remake (a fan project)
AppComments=A fan remake of MechWarrior (1989) on the MechWarrior 3 rebuild's engine; it needs your own copies of MechWarrior, MechWarrior 3 and Pirate's Moon.
DefaultDirName={autopf}\MechWarrior 1 Remake
; The folder page every time, an upgrade's too (the default hides it there), offering the folder already installed
; to.
DisableDirPage=no
DefaultGroupName=MechWarrior 1 Remake
DisableProgramGroupPage=yes
; Program Files needs administrator rights; a player without them can pick "only for me" (the user's own Programs
; folder) in the first dialog.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=mw1.ico
UninstallDisplayIcon={app}\mw1.exe
UninstallDisplayName=MechWarrior 1 Remake
OutputBaseFilename=MW1-Remake-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
#if FileExists(AddBackslash(Dist) + "LICENSE.txt")
LicenseFile={#Dist}\LICENSE.txt
#endif

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#Dist}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\MechWarrior 1 Remake"; Filename: "{app}\mw1.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\MechWarrior 1 Remake - Read me"; Filename: "{app}\README.txt"
Name: "{autodesktop}\MechWarrior 1 Remake"; Filename: "{app}\mw1.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\mw1.exe"; Description: "{cm:LaunchProgram,MechWarrior 1 Remake}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Only what the installer put in {app}; %APPDATA%\BattleTech\MechWarrior 1 Remake stays.
Type: dirifempty; Name: "{app}"

[Messages]
UninstalledAll=%1 was removed from your computer.%n%nYour settings and saved games, and where your games are installed, are kept in %APPDATA%\BattleTech\MechWarrior 1 Remake. Delete that folder to remove them too.
