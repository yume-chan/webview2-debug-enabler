// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <sstream>
#include <iostream>
#include <filesystem>
#include <format>
#include <DbgHelp.h>

typedef LPAPI_VERSION(NTAPI* PF_ImagehlpApiVersionEx)(_In_ LPAPI_VERSION AppVersion);

typedef BOOL(NTAPI* PF_SymInitialize)(_In_ HANDLE hProcess,
	_In_opt_ LPCSTR UserSearchPath,
	_In_ BOOL fInvadeProcess);
typedef DWORD(NTAPI* PF_SymSetOptions)(_In_ DWORD SymOptions);
typedef DWORD(NTAPI* PF_SymGetOptions)(VOID);
typedef DWORD64(NTAPI* PF_SymLoadModuleExW)(
	_In_ HANDLE        hProcess,
	_In_ HANDLE        hFile,
	_In_ PCWSTR        ImageName,
	_In_ PCWSTR        ModuleName,
	_In_ DWORD64       BaseOfDll,
	_In_ DWORD         DllSize,
	_In_ PMODLOAD_DATA Data,
	_In_ DWORD         Flags
	);
typedef BOOL(NTAPI* PF_SymGetModuleInfo64)(_In_ HANDLE hProcess,
	_In_ DWORD64 qwAddr,
	_Out_ PIMAGEHLP_MODULE64 ModuleInfo);
typedef BOOL(NTAPI* PF_SymFromName)(_In_ HANDLE hProcess,
	_In_ LPSTR Name,
	_Out_ PSYMBOL_INFO Symbol);
typedef BOOL(NTAPI* PF_SymSetSearchPath)(
	_In_ HANDLE hProcess,
	_In_opt_ PCSTR SearchPath
	);
typedef BOOL(NTAPI* PF_SymSrvIsStore)(
	_In_opt_ HANDLE hProcess,
	_In_ PCSTR path
	);
typedef BOOL(NTAPI* PF_SymRegisterCallback64)(
	_In_ HANDLE hProcess,
	_In_ PSYMBOL_REGISTERED_CALLBACK64 CallbackFunction,
	_In_ ULONG64 UserContext
	);

static std::wstring to_hex(uint64_t value) {
	std::wstringstream stream;
	stream << std::hex << (int64_t)value;
	return stream.str();
}

class StringPiece {
public:
	StringPiece(const char* s)
		:ptr(s), length(strlen(s)) {
	}

protected:
	const char* ptr;
	size_t length;
};

HMODULE get_current_module() {
	HMODULE module_handle = nullptr;
	GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCTSTR)get_current_module,
		&module_handle
	);
	return module_handle;
}

std::wstring get_module_file_name(HMODULE handle) {
	std::wstring module_path;
	module_path.resize((size_t)MAX_PATH);

	auto module_path_length = GetModuleFileName(
		handle,
		module_path.data(),
		(DWORD)module_path.capacity()
	);
	module_path.resize(module_path_length);

	return module_path;
}

uintptr_t find_function(std::wstring module_name, std::string name) {
	auto module_handle = GetModuleHandle(module_name.c_str());
	if (!module_handle) {
		return 0;
	}

	auto address = GetProcAddress(module_handle, name.c_str());
	if (address) {
		return (uintptr_t)address;
	}

	auto self_path = get_module_file_name(get_current_module());
	auto self_folder = std::filesystem::path(self_path).parent_path();
	auto dbgHelp = LoadLibraryEx(
		(self_folder / "dbghelp.dll").wstring().c_str(),
		nullptr,
		0
	);
	if (!dbgHelp) {
		MessageBox(nullptr, L"Can't find dbghelp.dll", L"Error", 0);
		return 0;
	}

	auto pfImagehlpApiVersionEx = (PF_ImagehlpApiVersionEx)GetProcAddress(dbgHelp, "ImagehlpApiVersionEx");
	auto pfSymInitialize = (PF_SymInitialize)GetProcAddress(dbgHelp, "SymInitialize");
	auto pfSymSetOptions = (PF_SymSetOptions)GetProcAddress(dbgHelp, "SymSetOptions");
	auto pfSymGetOptions = (PF_SymGetOptions)GetProcAddress(dbgHelp, "SymGetOptions");
	auto pfSymLoadModuleExW = (PF_SymLoadModuleExW)GetProcAddress(dbgHelp, "SymLoadModuleExW");
	auto pfSymGetModuleInfo64 = (PF_SymGetModuleInfo64)GetProcAddress(dbgHelp, "SymGetModuleInfo64");
	auto pfSymFromName = (PF_SymFromName)GetProcAddress(dbgHelp, "SymFromName");
	auto pfSymSetSearchPath = (PF_SymSetSearchPath)GetProcAddress(dbgHelp, "SymSetSearchPath");
	auto pfSymSrvIsStore = (PF_SymSrvIsStore)GetProcAddress(dbgHelp, "SymSrvIsStore");
	auto pfSymRegisterCallback64 = (PF_SymRegisterCallback64)GetProcAddress(dbgHelp, "SymRegisterCallback64");

	API_VERSION av{};
	av.MajorVersion = API_VERSION_NUMBER;
	pfImagehlpApiVersionEx(&av);
	if (av.MajorVersion < API_VERSION_NUMBER) {
		MessageBox(nullptr, L"Incorrect version of dbghelp.dll", L"Error", 0);
		FreeLibrary(dbgHelp);
		return 0;
	}

	auto process = GetCurrentProcess();
	auto symbol_path = std::format("srv*{}*https://msdl.microsoft.com/download/symbols", (self_folder / "symbols").string());
	if (!pfSymInitialize(
		process,
		symbol_path.c_str(),
		false
	)) {
		MessageBox(nullptr, L"SymInitialize failed", L"Error", 0);
		FreeLibrary(dbgHelp);
		return 0;
	}

#if _DEBUG
	pfSymSetOptions(SYMOPT_DEBUG);

	pfSymRegisterCallback64(
		process,
		[](auto process, auto action, auto data, auto context) {
			switch (action) {
			case CBA_DEBUG_INFO:
				MessageBoxA(nullptr, (char*)data, "DEBUG", 0);
				return TRUE;
			case CBA_DEFERRED_SYMBOL_LOAD_CANCEL:
			{
				auto info = (IMAGEHLP_DEFERRED_SYMBOL_LOAD64*)data;
				MessageBoxA(nullptr, info->FileName, "LOAD", 0);
				return FALSE;
			}
			case CBA_DEFERRED_SYMBOL_LOAD_FAILURE:
			{
				auto info = (IMAGEHLP_DEFERRED_SYMBOL_LOAD64*)data;
				MessageBoxA(nullptr, info->FileName, "LOAD_FAILURE", 0);
				return FALSE;
			}
			case CBA_EVENT:
			{
				auto info = (IMAGEHLP_CBA_EVENT*)data;
				MessageBoxA(nullptr, info->desc, "EVENT", 0);
				return TRUE;
			}
			case CBA_SRCSRV_INFO:
				MessageBoxA(nullptr, (char*)data, "SRCSRV", 0);
				return TRUE;
			}
			return FALSE;
		},
		0);
#endif

	std::wstring module_path = get_module_file_name(module_handle);
	if (!pfSymLoadModuleExW(process, nullptr, module_path.c_str(), nullptr, (DWORD64)module_handle, 0, nullptr, 0)) {
		if (GetLastError()) {
			MessageBox(nullptr, to_hex(GetLastError()).c_str(), L"SymLoadModuleExW", 0);
			FreeLibrary(dbgHelp);
			return 0;
		}
	}

	IMAGEHLP_MODULE64 module_info;
	memset(&module_info, 0, sizeof(module_info));
	module_info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
	if (!pfSymGetModuleInfo64(process, (DWORD64)module_handle, &module_info)) {
		auto error = GetLastError();
		MessageBox(nullptr, to_hex(error).c_str(), L"SymGetModuleInfo64", 0);
		FreeLibrary(dbgHelp);
		return 0;
	}

	auto symbol_name = std::format("{}!{}", module_info.ModuleName, name);
	SYMBOL_INFO symbol;
	memset(&symbol, 0, sizeof(symbol));
	symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
	if (!pfSymFromName(process, symbol_name.data(), &symbol)) {
		auto error = GetLastError();
		switch (error) {
		case ERROR_NOT_FOUND:
			MessageBox(nullptr, L"Symbol not found", L"SymFromName", 0);
			break;
		default:
			MessageBox(nullptr, to_hex(error).c_str(), L"SymFromName", 0);
			break;
		}
		FreeLibrary(dbgHelp);
		return 0;
	}

	return symbol.Address;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		auto msedge = GetModuleHandle(L"msedge.dll");

		auto command_line_for_current_process = (void* (*)())find_function(L"msedge.dll", "?ForCurrentProcess@CommandLine@base@@SAPEAV12@XZ");
		//auto command_line_for_current_process = (void* (*)())find_function(L"msedge.dll", "public: static class base::CommandLine * __ptr64 __cdecl base::CommandLine::ForCurrentProcess(void)");
		if (!command_line_for_current_process) {
			MessageBox(nullptr, L"Can't find CommandLine::ForCurrentProcess", L"Error", 0);
			return false;
		}
		// MessageBox(nullptr, to_hex((int64_t)command_line_for_current_process).c_str(), L"CommandLine::ForCurrentProcess", 0);

		auto command_line = command_line_for_current_process();
		//MessageBox(nullptr, to_hex((int64_t)command_line).c_str(), L"Got command line", 0);

		auto command_line_append_switch_ascii = (void(*)(void*, StringPiece, StringPiece))find_function(L"msedge.dll", "?AppendSwitchASCII@CommandLine@base@@QEAAXV?$BasicStringPiece@DU?$char_traits@D@__1@std@@@2@0@Z");
		// MessageBox(nullptr, to_hex((int64_t)command_line_append_switch_ascii).c_str(), L"CommandLine::AppendSwitchASCII", 0);
		if (!command_line_append_switch_ascii) {
			MessageBox(nullptr, L"Can't find CommandLine::AppendSwitchASCII", L"Error", 0);
			return false;
		}

		command_line_append_switch_ascii(command_line, StringPiece("remote-debugging-port"), StringPiece("9222"));
		// MessageBox(nullptr, L"Switch appended", L"Yeah!", 0);

		auto remote_debugging_server_ctor = (void* (*)(void*))find_function(L"msedge.dll", "??0RemoteDebuggingServer@@QEAA@XZ");
		if (!remote_debugging_server_ctor) {
			MessageBox(nullptr, L"RemoteDebuggingServer::RemoteDebuggingServer", L"Error", 0);
			return false;
		}
		auto ptr = malloc(8);
		// MessageBox(nullptr, to_hex((int64_t)remote_debugging_server_ctor).c_str(), L"RemoteDebuggingServer::RemoteDebuggingServer", 0);
		auto remote_debugging_server = remote_debugging_server_ctor(ptr);

		MessageBox(nullptr, L"Doesn't crash!", L"Yeah!", 0);
		return true;
	}
	break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

