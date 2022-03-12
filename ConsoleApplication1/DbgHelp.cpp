#include "DbgHelp.h"

DbgHelp::DbgHelp() {
	auto self_path = get_module_file_name(get_current_module());
	auto self_folder = std::filesystem::path(self_path).parent_path();
	_handle = LoadLibraryEx(
		(self_folder / "dbghelp.dll").wstring().c_str(),
		nullptr,
		0
	);
	if (!_handle) {
		MessageBox(nullptr, L"Can't find dbghelp.dll", L"Error", 0);
		throw std::exception();
	}

	_image_hlp_api_version_ex = (PF_ImagehlpApiVersionEx)GetProcAddress(_handle, "ImagehlpApiVersionEx");

	API_VERSION av{};
	av.MajorVersion = API_VERSION_NUMBER;
	_image_hlp_api_version_ex(&av);
	if (av.MajorVersion < API_VERSION_NUMBER) {
		MessageBox(nullptr, L"Incorrect version of dbghelp.dll", L"Error", 0);
		FreeLibrary(_handle);
		throw std::exception();
	}

	_process = GetCurrentProcess();
	auto symbol_path = std::format("srv*{}*https://msdl.microsoft.com/download/symbols", (self_folder / "symbols").string());
	_sym_initialize = (PF_SymInitialize)GetProcAddress(_handle, "SymInitialize");
	if (!_sym_initialize(
		_process,
		symbol_path.c_str(),
		false
	)) {
		MessageBox(nullptr, L"SymInitialize failed", L"Error", 0);
		FreeLibrary(_handle);
		throw std::exception();
	}

	_sym_load_module_ex_w = (PF_SymLoadModuleExW)GetProcAddress(_handle, "SymLoadModuleExW");
	_sym_get_module_info_64 = (PF_SymGetModuleInfo64)GetProcAddress(_handle, "SymGetModuleInfo64");
	_sym_from_name = (PF_SymFromName)GetProcAddress(_handle, "SymFromName");
}

void DbgHelp::load_module(std::filesystem::path path) {
	if (!_sym_load_module_ex_w(_process, nullptr, path.wstring().c_str(), nullptr, 0, 0, nullptr, 0)) {
		if (GetLastError()) {
			MessageBox(nullptr, to_hex(GetLastError()).c_str(), L"SymLoadModuleExW", 0);
			throw std::exception();
		}
	}
}

void DbgHelp::load_module(HMODULE handle) {
	std::wstring module_path = get_module_file_name(handle);
	if (!_sym_load_module_ex_w(_process, nullptr, module_path.c_str(), nullptr, (DWORD64)handle, 0, nullptr, 0)) {
		if (GetLastError()) {
			MessageBox(nullptr, to_hex(GetLastError()).c_str(), L"SymLoadModuleExW", 0);
			throw std::exception();
		}
	}
}

IMAGEHLP_MODULE64 DbgHelp::get_module_info(HMODULE handle) {
	IMAGEHLP_MODULE64 info;
	memset(&info, 0, sizeof(info));
	info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
	if (!_sym_get_module_info_64(_process, (DWORD64)handle, &info)) {
		auto error = GetLastError();
		MessageBox(nullptr, to_hex(error).c_str(), L"SymGetModuleInfo64", 0);
		throw std::exception();
	}
	return info;
}

SYMBOL_INFO DbgHelp::sym_from_name(std::string name) {
	SYMBOL_INFO symbol;
	memset(&symbol, 0, sizeof(symbol));
	symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
	if (!_sym_from_name(_process, name.data(), &symbol)) {
		auto error = GetLastError();
		switch (error) {
		case ERROR_NOT_FOUND:
			MessageBox(nullptr, L"Symbol not found", L"SymFromName", 0);
			break;
		default:
			MessageBox(nullptr, to_hex(error).c_str(), L"SymFromName", 0);
			break;
		}
		throw std::exception();
	}
	return symbol;
}

SYMBOL_INFO DbgHelp::sym_from_method_name(HMODULE handle, std::string name) {
	auto module_info = get_module_info(handle);
	auto symbol_name = std::format("{}!{}", module_info.ModuleName, name);
	return sym_from_name(symbol_name);
}
