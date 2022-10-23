/**
 * runs the vm
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 19-jun-2022
 * @license see 'LICENSE' file
 */

#include "vm.h"
#include "core/timer.h"

#include <vector>
#include <optional>
#include <iostream>
#include <fstream>

#if __has_include(<filesystem>)
	#include <filesystem>
	namespace fs = std::filesystem;
#elif __has_include(<boost/filesystem.hpp>)
	#include <boost/filesystem.hpp>
	namespace fs = boost::filesystem;
#endif

#include <boost/program_options.hpp>
namespace args = boost::program_options;


struct VMOptions
{
	t_vm_addr mem_size { 4096 };
	std::optional<t_vm_addr> frame_size { std::nullopt };
	bool enable_debug { false };
	bool zero_mem { false };
	bool enable_memimages { false };
	bool enable_checks { true };
};



static bool run_vm(const fs::path& prog, const VMOptions& opts)
{
	std::size_t filesize = fs::file_size(prog);
	std::ifstream ifstr(prog, std::ios_base::binary);
	if(!ifstr)
		return false;

	std::vector<VM::t_byte> bytes;
	bytes.resize(filesize);

	ifstr.read(reinterpret_cast<char*>(bytes.data()), filesize);
	if(ifstr.fail())
		return false;

	VM vm(opts.mem_size, opts.frame_size);
	VM::t_addr sp_initial = vm.GetSP();

	vm.SetDebug(opts.enable_debug);
	vm.SetChecks(opts.enable_checks);
	vm.SetZeroPoppedVals(opts.zero_mem);
	vm.SetDrawMemImages(opts.enable_memimages);
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
				std::cout << val << " [" << type_name << "]";
		}, dat);
		std::cout << std::endl;

		++stack_idx;
	}

	return true;
}



int main(int argc, char** argv)
{
	try
	{
		std::ios_base::sync_with_stdio(false);

                // --------------------------------------------------------------------
                // get program arguments
                // --------------------------------------------------------------------
		std::vector<std::string> progs;
		VMOptions vmopts
		{
			.mem_size = 4096,
			.frame_size = std::nullopt,
			.enable_debug = false,
			.zero_mem = false,
			.enable_memimages = false,
			.enable_checks = true,
		};
		typename decltype(vmopts.frame_size)::value_type frame_size = -1;
		bool enable_timer = false;

		args::options_description arg_descr("Virtual machine arguments");
		arg_descr.add_options()
			("debug,d", args::bool_switch(&vmopts.enable_debug), "enable debug output")
			("timer,t", args::bool_switch(&enable_timer), "time code execution")
			("zeromem,z", args::bool_switch(&vmopts.zero_mem), "zero memory after use")
#ifdef USE_BOOST_GIL
			("memimages,i", args::bool_switch(&vmopts.enable_memimages), "write memory images")
#endif
			("checks,c", args::value<bool>(&vmopts.enable_checks), "enable memory checks")
			("mem,m", args::value<decltype(vmopts.mem_size)>(&vmopts.mem_size), "set memory size")
			("frame,f", args::value<decltype(frame_size)>(&frame_size), "set stack frame size")
			("prog", args::value<decltype(progs)>(&progs), "input program to run");

		args::positional_options_description posarg_descr;
		posarg_descr.add("prog", -1);

		auto argparser = args::command_line_parser{argc, argv};
		argparser.style(args::command_line_style::default_style);
		argparser.options(arg_descr);
		argparser.positional(posarg_descr);

		args::variables_map mapArgs;
		auto parsedArgs = argparser.run();
		args::store(parsedArgs, mapArgs);
		args::notify(mapArgs);

		if(progs.size() == 0)
		{
			std::cout << "Script virtual machine"
				<< " by Tobias Weber <tobias.weber@tum.de>, 2022."
				<< std::endl;
			std::cout << "Internal data type lengths:"
				<< " real: " << sizeof(t_vm_real)*8 << " bits,"
				<< " int: " << sizeof(t_vm_int)*8 << " bits,"
				<< " address: " << sizeof(t_vm_addr)*8 << " bits."
				<< std::endl;

			std::cerr << "Please specify an input program.\n" << std::endl;
			std::cout << arg_descr << std::endl;
			return 0;
		}
                // --------------------------------------------------------------------

                // input file
		fs::path inprog = progs[0];

		t_timepoint start_time {};
		if(enable_timer)
			start_time  = t_clock::now();

		if(frame_size >= 0)
			vmopts.frame_size = frame_size;

		if(!run_vm(inprog, vmopts))
		{
			std::cerr << "Could not run \"" << inprog.string()
				<< "\"." << std::endl;
			return -1;
		}

		if(enable_timer)
		{
			auto [run_time, time_unit] = get_elapsed_time<
				t_real, t_timepoint>(start_time);
			std::cout << "Program run time: "
				<< run_time << " " << time_unit << "."
				<< std::endl;
		}

		if(vmopts.enable_memimages)
		{
			std::cout << "Create memory access video using: "
				<< "\"ffmpeg -i mem_%d.png mem.mp4\""
				<< "." << std::endl;
		}
	}
	catch(const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
		return -1;
	}

	return 0;
}
