; Inno Setup Script for AgenticAI
; Compiles the standalone AgenticAI-Setup.exe installer

[Setup]
AppName=AgenticAI
AppVersion=1.0.0
AppPublisher=AgenticAI Team
DefaultDirName={autopf}\AgenticAI
DefaultGroupName=AgenticAI
UninstallDisplayIcon={app}\AgenticAI.exe
Compression=lzma2/ultra64
SolidCompression=yes
OutputDir=Output
OutputBaseFilename=AgenticAI-Setup
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\AgenticAI"; Filename: "{app}\AgenticAI.exe"
Name: "{userdesktop}\AgenticAI"; Filename: "{app}\AgenticAI.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AgenticAI.exe"; Description: "{cm:LaunchProgram,AgenticAI}"; Flags: nowait postinstall skipifsilent
