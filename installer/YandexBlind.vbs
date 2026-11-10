Option Explicit
Dim shell, fso, base, browser, profile, args
Dim aiServer, aiModel, aiCommand, wmi, processes, process, found
Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
base = fso.GetParentFolderName(WScript.ScriptFullName)

browser = """" & base & "\Browser\chrome.exe" & """"
profile = """" & shell.ExpandEnvironmentStrings("%LOCALAPPDATA%") & "\YandexBlind\Profile" & """"
aiServer = base & "\AI\llama-server.exe"
aiModel = base & "\AI\models\Qwen3.5-0.8B-Q4_0.gguf"

' Start the local model engine silently, bound to loopback only.
If fso.FileExists(aiServer) And fso.FileExists(aiModel) Then
  found = False
  On Error Resume Next
  Set wmi = GetObject("winmgmts:\\.\root\cimv2")
  Set processes = wmi.ExecQuery("SELECT ExecutablePath FROM Win32_Process WHERE Name='llama-server.exe'")
  For Each process In processes
    If LCase(CStr(process.ExecutablePath)) = LCase(aiServer) Then found = True
  Next
  On Error GoTo 0
  If Not found Then
    aiCommand = """" & aiServer & """ -m """ & aiModel & """ --host 127.0.0.1 --port 8765 --ctx-size 2048 --threads 4 --n-gpu-layers 0"
    shell.Run aiCommand, 0, False
  End If
End If

args = " --user-data-dir=" & profile & " --load-extension=""" & base & "\NewTab""" & _
       " --no-first-run --disable-metrics --disable-metrics-reporting" & _
       " --disable-background-networking --disable-component-update --disable-domain-reliability" & _
       " --disable-sync --disable-breakpad --disable-crash-reporter --disable-background-mode" & _
       " --disable-default-apps chrome://newtab/"

' Keep the launcher alive while the browser is open.
shell.Run browser & args, 1, True

' Stop only this bundled local inference engine when the browser exits.
On Error Resume Next
Set wmi = GetObject("winmgmts:\\.\root\cimv2")
Set processes = wmi.ExecQuery("SELECT * FROM Win32_Process WHERE Name='llama-server.exe'")
For Each process In processes
  If LCase(CStr(process.ExecutablePath)) = LCase(aiServer) Then process.Terminate
Next
On Error GoTo 0
