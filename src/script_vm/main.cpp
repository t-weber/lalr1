/**
 * run vm
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 19-jun-2022
 * @license see 'LICENSE' file
 */

#include "vm.h"

#include <vector>
#include <iostream>
#include <fstream>

#if __has_include(<filesystem>)
	#include <filesystem>
	namespace fs = std::filesystem;
#elif __has_include(<boost/filesystem.hpp>)
	#include <boost/filesystem.hpp>
	namespace fs = boost::filesystem;
#endif



static bool run_vm(const char* _prog = nullptr)
{
	fs::path prog = _prog;
	std::size_t filesize = fs::file_size(prog);

	std::ifstream ifstr(prog, std::ios_base::binary);
	if(!ifstr)
		return false;

	std::vector<VM::t_byte> bytes;
	bytes.resize(filesize);

	ifstr.read(reinterpret_cast<char*>(bytes.data()), filesize);
	if(ifstr.fail())
		return false;

	try
	{
		VM vm(4096);
		VM::t_addr sp_initial = vm.GetSP();

		vm.SetDebug(false);
		vm.SetChecks(true);
		vm.SetMem(0, bytes.data(), filesize, true);
		vm.Run();

		// print remaining stack
		std::size_t stack_idx = 0;
		while(vm.GetSP() < sp_initial)
		{
			VM::t_data dat = vm.PopData();
			const char* type_name = VM::GetDataTypeName(dat);

			std::cout << "Stack[" << stack_idx << "] = ";
			std::visit([type_name](auto&& val) -> void
			{
				using t_val = std::decay_t<decltype(val)>;
				// variant not empty?
				if constexpr(!std::is_same_v<t_val, std::monostate>)
				{
					std::cout << val
						<< " ["
						<< type_name
						<< "]";
				}
			}, dat);
			std::cout << std::endl;

			++stack_idx;
		}
	}
	catch(const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
		return false;
	}

	return true;
}



int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	std::ios_base::sync_with_stdio(false);

	if(argc <= 1)
	{
		std::cerr << "Please give a compiled program." << std::endl;
		return -1;
	}

	if(!run_vm(argv[1]))
	{
		std::cerr << "Could not run \"" << argv[1] << "\"." << std::endl;
		return -1;
	}

	return 0;
}
