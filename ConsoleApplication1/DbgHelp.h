#pragma once

#include <filesystem>
#include <format>

#include <Windows.h>
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

static HMODULE get_current_module() {
	HMODULE module_handle = nullptr;
	GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCTSTR)get_current_module,
		&module_handle
	);
	return module_handle;
}

static std::wstring get_module_file_name(HMODULE handle) {
	std::wstring module_path;
	module_path.resize((size_t)MAX_PATH);

	auto module_path_length = GetModuleFileName(
		handle,
		module_path.data(),
		(DWORD)module_path.length()
	);
	module_path.resize(module_path_length);

	return module_path;
}

static std::wstring to_hex(uint64_t value) {
	std::wstringstream stream;
	stream << std::hex << (int64_t)value;
	return stream.str();
}

class DbgHelp {
public:
	DbgHelp();

	void load_module(std::filesystem::path path);
	void load_module(HMODULE handle);

	IMAGEHLP_MODULE64 get_module_info(HMODULE handle);

	SYMBOL_INFO sym_from_name(std::string name);
	SYMBOL_INFO sym_from_method_name(HMODULE handle, std::string name);

private:
	HMODULE _handle;
	HANDLE _process;

	PF_ImagehlpApiVersionEx _image_hlp_api_version_ex;
	PF_SymInitialize _sym_initialize;
	PF_SymLoadModuleExW _sym_load_module_ex_w;
	PF_SymGetModuleInfo64 _sym_get_module_info_64;
	PF_SymFromName _sym_from_name;
};
