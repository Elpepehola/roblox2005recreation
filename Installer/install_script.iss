;InnoSetupVersion=5.4.3
;ONLY USE THIS IF YOU COMPILED WITH VISUAL STUDIO 2005!!!
#define AppVer GetFileVersion('..\Roblox.exe')

[Setup]
AppName=Roblox
AppVersion=v{#AppVer}
AppId={{4C5DF268-0208-4CDE-A7F0-65F7E2CB5067}
AppPublisherURL=http://www.robiox.bid/
AppSupportURL=http://www.robiox.bid/
AppUpdatesURL=http://www.robiox.bid/
DefaultDirName={%localappdata}\Roblox
OutputBaseFilename=Roblox_Setup_v{#AppVer}
Compression=lzma2
PrivilegesRequired=lowest
WizardImageFile=setup.bmp
DefaultGroupName=Roblox


[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Files]
Source: "Redist\vcredist_x86.exe"; DestDir: "{tmp}"; Flags: ignoreversion 
;Source: "Redist\vcredist_x64.exe"; DestDir: "{tmp}"; Check: "IsWin64"; Flags: ignoreversion 
Source: "..\content\*"; DestDir: "{app}\content"; Flags: ignoreversion recursesubdirs
;Source: "..\SDL.DLL"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs
Source: "..\Roblox.exe"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Registry]


[Run]
Filename: "{tmp}\vcredist_x86.exe"; Parameters: "/q"; Tasks: instvc; 
;Filename: "{tmp}\vcredist_x64.exe"; Parameters: "/q"; Tasks: instvc; Check: "IsWin64";
Filename: "iexplore.exe"; Parameters: "http://www.robiox.bid/FirstInstall"; Description: Start playing Roblox; Flags: shellexec postinstall nowait skipifsilent

[Icons]
Name: "{group}\Play Roblox"; Filename: "{%programfiles}\Internet Explorer\iexplore.exe"; Parameters: "http://www.robiox.bid/Games"; IconFilename: "{app}\Roblox.exe"; Tasks: startscut;
Name: "{group}\Roblox Editor"; Filename: "{app}\Roblox.exe"; Tasks: startscut;

Name: "{userdesktop}\Play Roblox"; Filename: "{%programfiles}\Internet Explorer\iexplore.exe"; Parameters: "http://www.robiox.bid/Games"; IconFilename: "{app}\Roblox.exe"; Tasks: startscut;
Name: "{userdesktop}\Roblox Editor"; Filename: "{app}\Roblox.exe"; Tasks: desktopicon

[Tasks]
Name: "instvc"; Description: "Install Visual C++ Redistributable 2005 SP1 (Requires elevated permissions)";
Name: "desktopicon"; Description: "Create Desktop Icons";
Name: "startscut"; Description: "Create Start Menu Icons";

