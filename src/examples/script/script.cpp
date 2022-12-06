/**
 * script compiler example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-jun-2022
 * @license see 'LICENSE' file
 */

#include "script_grammar.h"
#include "core/collection.h"
#include "core/timer.h"
#include "lexer.h"
#include "ast.h"
#include "ast_printer.h"
#include "ast_asm.h"
#include "script_vm/vm.h"

#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstdint>

#include <boost/program_options.hpp>
namespace args = boost::program_options;


using namespace lalr1;


#if __has_include(<filesystem>)
	#include <filesystem>
	namespace fs = std::filesystem;
#elif __has_include(<boost/filesystem.hpp>)
	#include <boost/filesystem.hpp>
	namespace fs = boost::filesystem;
#else
	#error No filesystem support found.
#endif


#if __has_include("script_parser.h")
	#include "script_parser.h"
	#include "script_parser.cpp"

	#define USE_RECASC 1

#elif __has_include("script.tab")
	#include "core/parser.h"
	#include "script.tab"

	#define USE_RECASC 0

#else
	static std::tuple<bool, std::string>
	lalr1_run_parser([[maybe_unused]] const std::string& script_file,
		[[maybe_unused]] bool debug_codegen = false,
		[[maybe_unused]] bool debug_parser = false)
	{
		std::cerr << "No parsing tables available, please\n"
			"\t- run \"./script_parsergen\" first,\n"
			"\t- \"touch " __FILE__ "\", and\n"
			"\t- rebuild using \"make\"."
			<< std::endl;

		return std::make_tuple(false, "");
	}

	#define __LALR_NO_PARSER_AVAILABLE
#endif



#ifndef __LALR_NO_PARSER_AVAILABLE


static std::tuple<bool, std::string>
lalr1_run_parser(const std::string& script_file,
	bool debug_codegen = false, bool debug_parser = false)
{
	try
	{
		ScriptGrammar grammar;
		grammar.CreateGrammar(false, true);
		const auto& rules = grammar.GetSemanticRules();

#if USE_RECASC != 0
		ScriptParser parser;
#else
		// get created parsing tables
		auto [shift_tab, reduce_tab, jump_tab, num_rhs, lhs_idx] = get_lalr1_tables();
		auto [term_idx, nonterm_idx, semantic_idx] = get_lalr1_table_indices();
		auto [err_idx, acc_idx, eps_id, end_id, start_idx, acc_rule_idx] = get_lalr1_constants();

		Parser parser;
		parser.SetShiftTable(shift_tab);
		parser.SetReduceTable(reduce_tab);
		parser.SetJumpTable(jump_tab);
		parser.SetSemanticIdxMap(semantic_idx);
		parser.SetNumRhsSymsPerRule(num_rhs);
		parser.SetLhsIndices(lhs_idx);
		parser.SetEndId(end_id);
		parser.SetStartingState(start_idx);
		parser.SetAcceptingRule(acc_rule_idx);
#endif
		parser.SetSemanticRules(&rules);
		parser.SetDebug(debug_parser);

		bool loop_input = true;
		while(loop_input)
		{
			std::unique_ptr<std::istream> istr;

			if(script_file != "")
			{
				// read script from file
				istr = std::make_unique<std::ifstream>(script_file);
				loop_input = false;

				if(!*istr)
				{
					std::cerr << "Error: Cannot open file \""
						<< script_file << "\"." << std::endl;
					return std::make_tuple(false, "");
				}

				std::cout << "Compiling \"" << script_file << "\"..."
					<< std::endl;
			}
			else
			{
				// read statements from command line
				std::cout << "\nStatement: ";
				std::string script;
				std::getline(std::cin, script);
				istr = std::make_unique<std::istringstream>(script);
			}

			// tokenise script
			Lexer lexer(istr.get());
#if USE_RECASC == 0
			lexer.SetTermIdxMap(term_idx);
#endif
			lexer.SetEndOnNewline(script_file == "");
			auto tokens = lexer.GetAllTokens();

			if(debug_codegen)
			{
				std::cout << "\nTokens: ";
				for(const t_toknode& tok : tokens)
				{
					std::size_t tokid = tok->GetId();
					if(tokid == (std::size_t)Token::END)
						std::cout << "END";
					else
						std::cout << tokid;
					std::cout << " ";
				}
				std::cout << std::endl;
			}

			::t_astbaseptr ast = std::dynamic_pointer_cast<::ASTBase>(parser.Parse(tokens));
			ast->AssignLineNumbers();
			ast->DeriveDataType();

			if(debug_codegen)
			{
				std::cout << "\nAST:\n";
				ASTPrinter printer{std::cout};
				ast->accept(&printer);
			}

			std::unordered_map<std::size_t, std::tuple<std::string, OpCode>> ops
			{{
				std::make_pair('+', std::make_tuple("add", OpCode::ADD)),
				std::make_pair('-', std::make_tuple("sub", OpCode::SUB)),
				std::make_pair('*', std::make_tuple("mul", OpCode::MUL)),
				std::make_pair('/', std::make_tuple("div", OpCode::DIV)),
				std::make_pair('%', std::make_tuple("mod", OpCode::MOD)),
				std::make_pair('^', std::make_tuple("pow", OpCode::POW)),

				std::make_pair('=', std::make_tuple("wrmem", OpCode::WRMEM)),

				std::make_pair('&', std::make_tuple("binand", OpCode::BINAND)),
				std::make_pair('|', std::make_tuple("binand", OpCode::BINOR)),
				std::make_pair('~', std::make_tuple("binand", OpCode::BINNOT)),

				std::make_pair('>', std::make_tuple("gt", OpCode::GT)),
				std::make_pair('<', std::make_tuple("lt", OpCode::LT)),
				std::make_pair(static_cast<std::size_t>(Token::EQU),
					std::make_tuple("equ", OpCode::EQU)),
				std::make_pair(static_cast<std::size_t>(Token::NEQU),
					std::make_tuple("nequ", OpCode::NEQU)),
				std::make_pair(static_cast<std::size_t>(Token::GEQU),
					std::make_tuple("gequ", OpCode::GEQU)),
				std::make_pair(static_cast<std::size_t>(Token::LEQU),
					std::make_tuple("lequ", OpCode::LEQU)),
				std::make_pair(static_cast<std::size_t>(Token::AND),
					std::make_tuple("and", OpCode::AND)),
				std::make_pair(static_cast<std::size_t>(Token::OR),
					std::make_tuple("or", OpCode::OR)),

				std::make_pair(static_cast<std::size_t>(Token::BIN_XOR),
					std::make_tuple("binxor", OpCode::BINXOR)),
				std::make_pair(static_cast<std::size_t>(Token::SHIFT_LEFT),
					std::make_tuple("shl", OpCode::SHL)),
				std::make_pair(static_cast<std::size_t>(Token::SHIFT_RIGHT),
					std::make_tuple("shr", OpCode::SHR)),
			}};

			std::ostringstream ostrAsm;
			if(debug_codegen)
			{
				ASTAsm astasm{ostrAsm, &ops};
				ast->accept(&astasm);
			}

			std::ostringstream ostrAsmBin(
				std::ios_base::out | std::ios_base::binary);
			ASTAsm astasmbin{ostrAsmBin, &ops};
			astasmbin.SetBinary(true);
			ast->accept(&astasmbin);
			astasmbin.PatchFunctionAddresses();
			astasmbin.FinishCodegen();
			std::string strAsmBin = ostrAsmBin.str();

			if(debug_codegen)
			{
				std::cout << "\nSymbol table:\n";
				std::cout << astasmbin.GetSymbolTable();

				std::cout << "\nGenerated code ("
					<< strAsmBin.size() << " bytes):\n"
					<< ostrAsm.str()
					<< std::endl;
			}

			fs::path binfile(script_file != "" ? script_file : "script.scr");
			binfile = binfile.filename();
			binfile.replace_extension(".bin");

			std::ofstream ofstrAsmBin(binfile.string(), std::ios_base::binary);
			if(!ofstrAsmBin)
			{
				std::cerr << "Cannot open \""
					<< binfile.string() << "\"." << std::endl;
				return std::make_tuple(false, "");
			}
			ofstrAsmBin.write(strAsmBin.data(), strAsmBin.size());
			if(ofstrAsmBin.fail())
			{
				std::cerr << "Cannot write \""
					<< binfile.string() << "\"." << std::endl;
				return std::make_tuple(false, "");
			}
			ofstrAsmBin.flush();

			std::cout << "Created compiled program \""
				<< binfile.string() << "\"." << std::endl;

			return std::make_tuple(true, strAsmBin);
		}
	}
	catch(const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
		return std::make_tuple(false, "");
	}

	return std::make_tuple(true, "");
}

#endif  // __LALR_NO_PARSER_AVAILABLE



static bool run_vm(const std::string& prog)
{
	VM vm(65536);
	//vm.SetDebug(true);
	//vm.SetZeroPoppedVals(true);
	//vm.SetDrawMemImages(true);
	VM::t_addr sp_initial = vm.GetSP();
	vm.SetMem(0, prog, true);
	vm.Run();

	if(vm.GetSP() != sp_initial)
	{
		std::cout << "\nResult: ";
		std::visit([](auto&& val) -> void
		{
			using t_val = std::decay_t<decltype(val)>;
			if constexpr(!std::is_same_v<t_val, std::monostate>)
				std::cout << val;
			else
				std::cout << "<none>";
		}, vm.TopData());
		std::cout << std::endl;
	}

	return true;
}



int main(int argc, char** argv)
{
	std::ios_base::sync_with_stdio(false);

	// --------------------------------------------------------------------
	// get program arguments
	// --------------------------------------------------------------------
	std::vector<std::string> progs;

	bool debug_codegen = false;
	bool debug_parser = false;
	bool runvm = false;

	args::options_description arg_descr("Script compiler arguments");
	arg_descr.add_options()
	("run,r", args::bool_switch(&runvm), "directly run the compiled program")
	("debug,d", args::bool_switch(&debug_codegen), "enable debug output for code generation")
	("debugparser,p", args::bool_switch(&debug_parser), "enable debug output for parser")
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
		std::cout << "Script compiler"
			<< " by Tobias Weber <tobias.weber@tum.de>, 2022."
			<< std::endl;
		std::cout << "Internal data type lengths:"
			<< " real: " << sizeof(t_real)*8 << " bits,"
			<< " int: " << sizeof(t_int)*8 << " bits,"
			<< " address: " << sizeof(VM::t_addr)*8 << " bits."
			<< std::endl;

		std::cerr << "Please specify an input script to compile.\n" << std::endl;
		std::cout << arg_descr << std::endl;
		return 0;
	}
	// --------------------------------------------------------------------

	t_timepoint start_codegen = t_clock::now();
	fs::path script_file = progs[0];

	if(auto [code_ok, prog] = lalr1_run_parser(
		script_file.string(), debug_codegen, debug_parser);
		code_ok)
	{
		auto [run_time, time_unit] = get_elapsed_time<
			t_real, t_timepoint>(start_codegen);
		std::cout << "Code generation time: "
			<< run_time << " " << time_unit << "."
			<< std::endl;
		if(runvm)
		{
			t_timepoint start_vm = t_clock::now();
			if(run_vm(prog))
			{
				auto [vm_run_time, vm_time_unit] = get_elapsed_time<
					t_real, t_timepoint>(start_vm);
				std::cout << "VM execution time: "
					<< vm_run_time << " " << vm_time_unit << "."
					<< std::endl;
			}
		}
	}

	return 0;
}
