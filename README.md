# WebView2 Debug Enabler

A proof-of-concept project for enabling remote debugging on running Microsoft Edge/WebView2.

The original goal is to enable modification in Windows 11's (useless) Widgets.

## Working Principle

It consists of two parts:

### Injector

1. Accept process ID from input
2. Find the full path of `msedge.dll` loaded in target process
3. Download debug symbol (PDB) file for it from Microsoft Symbol Server
4. Inject payload into target process using remote thread

### Payload

1. Load downloaded PDB file for `msedge.dll`
2. Find addresses of some un-exported functions
3. Call them

More precisely, it relies on 3 functions:

* `CommandLine::ForCurrentProcess`
* `CommandLine::AppendSwitchASCII`
* `RemoteDebuggingServer::RemoteDebuggingServer` (constructor)

I'm too lazy to link them to source code, please search on [cs.chromium.org](https://cs.chromium.org).

## Notes

1. Why not directly downloading PDB in payload?

    Because Chromium browser process has no Internet access.

## Build

1. Build normally in Visual Studio
2. Copy `dbghelp.dll`, `symsrv.dll` and `symsrv.yes` to output folder
