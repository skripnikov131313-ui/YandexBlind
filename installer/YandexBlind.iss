[Setup]
AppId={{D84B52E7-29A7-47CB-9C37-5C73E8F4C1B2}
AppName=Yandex Blind
AppVersion=0.1.0
AppPublisher=Yandex Blind Project
DefaultDirName={localappdata}\Programs\YandexBlind
DefaultGroupName=Yandex Blind
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\dist
OutputBaseFilename=YandexBlind-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\Browser\chrome.exe
CloseApplications=yes
RestartApplications=no

[Files]
Source: "..\dist\payload\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Yandex Blind"; Filename: "{sys}\wscript.exe"; Parameters: "//B //Nologo ""{app}\YandexBlind.vbs"""; WorkingDir: "{app}"
Name: "{autodesktop}\Yandex Blind"; Filename: "{sys}\wscript.exe"; Parameters: "//B //Nologo ""{app}\YandexBlind.vbs"""; WorkingDir: "{app}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные параметры:"
