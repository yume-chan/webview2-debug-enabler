// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>

#include <Windows.h>
#include <Psapi.h>

#include "DbgHelp.h"

std::wstring get_module_file_name_ex(HANDLE process, HMODULE handle) {
	std::wstring module_path;
	module_path.resize((size_t)MAX_PATH);

	auto module_path_length = GetModuleFileNameEx(
		process,
		handle,
		module_path.data(),
		(DWORD)module_path.length()
	);
	module_path.resize(module_path_length);

	return module_path;
}

int main()
{
	int pid;
	std::cout << "process id of msedge.exe/msedgewebview2.exe: ";
	std::cin >> pid;

	auto process = OpenProcess(
		PROCESS_QUERY_INFORMATION |
		PROCESS_VM_READ |
		PROCESS_VM_WRITE |
		PROCESS_VM_OPERATION |
		PROCESS_CREATE_THREAD,
		false,
		pid
	);
	if (!process) {
		auto error = GetLastError();
		std::cout << "OpenProcess failed! code: 0x" << std::hex << error << std::endl;
		return -1;
	}

	HMODULE modules[1024];
	DWORD needed;
	if (!EnumProcessModules(process, modules, sizeof(modules), &needed)) {
		auto error = GetLastError();
		std::cout << "EnumProcessModules failed! code: 0x" << std::hex << error << std::endl;
		return -1;
	}

	std::wstring msedge_path;
	for (auto i = 0; i < needed / sizeof(HMODULE); i++) {
		std::wstring base_name;
		base_name.resize(MAX_PATH);
		auto size = GetModuleBaseName(
			process,
			modules[i],
			base_name.data(),
			(DWORD)base_name.length()
		);
		base_name.resize(size);

		if (base_name == L"msedge.dll") {
			msedge_path = get_module_file_name_ex(process, modules[i]);
			break;
		}
	}

	if (!msedge_path.length()) {
		std::cout << "Can't find msedge.dll in target process" << std::endl;
		return -1;
	}

	DbgHelp dbg_help;
	dbg_help.load_module(std::filesystem::path(msedge_path));
	std::cout << "Symbol loaded" << std::endl;

	auto kernel32 = GetModuleHandle(L"kernel32.dll");
	if (!kernel32) {
		std::cout << "kernel32 not found!" << std::endl;
		return -1;
	}

	auto load_library = GetProcAddress(kernel32, "LoadLibraryW");
	if (!load_library) {
		std::cout << "LoadLibraryW not found!" << std::endl;
		return -1;
	}

	auto self_path = get_module_file_name(get_current_module());
	auto self_folder = std::filesystem::path(self_path).parent_path();
	auto dll_name = (self_folder / "Dll1.dll").wstring();
	auto dll_name_length = (dll_name.size() + 1) * sizeof(std::wstring::traits_type::char_type);

	auto address = VirtualAllocEx(process, nullptr, dll_name_length, MEM_COMMIT, PAGE_READWRITE);
	if (!address) {
		auto error = GetLastError();
		std::cout << "VirtualAllocEx failed! code: 0x" << std::hex << error << std::endl;
		return -1;
	}

	WriteProcessMemory(process, address, dll_name.c_str(), dll_name_length, nullptr);

	auto thread = CreateRemoteThread(process, nullptr, 0, (LPTHREAD_START_ROUTINE)load_library, address, 0, nullptr);

	std::cout << "Hello World!\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
