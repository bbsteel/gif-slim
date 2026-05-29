[Setup]
AppId={{7A39E0F8-2EA9-45A0-B894-3C0A6B1A7B5B}
AppName=GIF Slim
AppVersion=1.0.0
DefaultDirName={autopf}\GIF Slim
DefaultGroupName=GIF Slim
Compression=lzma
SolidCompression=yes
OutputDir=dist
OutputBaseFilename=GifSlim-1.0.0-windows-x64-setup
WizardStyle=modern

[Files]
Source: "dist\windows-portable\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\GIF Slim"; Filename: "{app}\gif-slim.exe"
Name: "{autodesktop}\GIF Slim"; Filename: "{app}\gif-slim.exe"

[Run]
Filename: "{app}\gif-slim.exe"; Description: "Launch GIF Slim"; Flags: nowait postinstall skipifsilent
