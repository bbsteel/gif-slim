[Setup]
AppId={{7A39E0F8-2EA9-45A0-B894-3C0A6B1A7B5B}
AppName=GIF Editor
AppVersion=1.0.0
DefaultDirName={autopf}\GIF Editor
DefaultGroupName=GIF Editor
Compression=lzma
SolidCompression=yes
OutputDir=dist
OutputBaseFilename=GifEditor-1.0.0-windows-x64-setup
WizardStyle=modern

[Files]
Source: "dist\windows-portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\GIF Editor"; Filename: "{app}\gif-editor.exe"
Name: "{autodesktop}\GIF Editor"; Filename: "{app}\gif-editor.exe"

[Run]
Filename: "{app}\gif-editor.exe"; Description: "Launch GIF Editor"; Flags: nowait postinstall skipifsilent
