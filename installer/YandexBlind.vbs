Option Explicit
Dim shell, fso, base, browser, profile, args
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
base = fso.GetParentFolderName(WScript.ScriptFullName)
browser = """" & base & "\Browser\chrome.exe" & """"
profile = """" & shell.ExpandEnvironmentStrings("%LOCALAPPDATA%") & "\YandexBlind\Profile" & """"
args = " --user-data-dir=" & profile & " --homepage=https://ya.ru --load-extension=""" & base & "\NewTab" & """ --no-first-run --disable-metrics --disable-metrics-reporting"
shell.Run browser & args, 1, False
