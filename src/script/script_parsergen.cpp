/**
 * script compiler generator example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-jun-2022
 * @license see 'LICENSE' file
 */

#include "script_grammar.h"
#include "lalr1/collection.h"
#include "common/helpers.h"
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


static bool lr1_create_parser(
	bool create_ascent_parser = true, bool create_tables = false,
	bool debug = false, bool write_graph = false)
{
	try
	{
		ScriptGrammar grammar;
		grammar.CreateGrammar(true, false);
		NonTerminalPtr start = grammar.GetStartNonTerminal();

		if(debug)
		{
			std::vector<NonTerminalPtr> all_nonterminals = grammar.GetAllNonTerminals();

			std::cout << "Productions:\n";
			for(NonTerminalPtr nonterm : all_nonterminals)
				nonterm->print(std::cout);
			std::cout << std::endl;

			std::cout << "FIRST sets:\n";
			t_map_first first;
			t_map_first_perrule first_per_rule;
			for(const NonTerminalPtr& nonterminal : all_nonterminals)
				nonterminal->CalcFirst(first, &first_per_rule);

			for(const auto& pair : first)
			{
				std::cout << pair.first->GetStrId() << ": ";
				for(const auto& _first : pair.second)
					std::cout << _first->GetStrId() << " ";
				std::cout << "\n";
			}
			std::cout << std::endl;


			std::cout << "FOLLOW sets:\n";
			t_map_follow follow;
			for(const NonTerminalPtr& nonterminal : all_nonterminals)
				nonterminal->CalcFollow(all_nonterminals, start, first, follow);

			for(const auto& pair : follow)
			{
				std::cout << pair.first->GetStrId() << ": ";
				for(const auto& _first : pair.second)
					std::cout << _first->GetStrId() << " ";
				std::cout << "\n";
			}
			std::cout << std::endl;
		}

		ElementPtr elem = std::make_shared<Element>(
			start, 0, 0, Terminal::t_terminalset{{ g_end }});
		ClosurePtr closure = std::make_shared<Closure>();
		closure->AddElement(elem);

		auto progress = [](const std::string& msg, [[maybe_unused]] bool done)
		{
			std::cout << "\r\x1b[K" << msg;
			if(done)
				std::cout << "\n";
			std::cout << std::flush;
		};

		Collection collsLALR{ closure };
		collsLALR.SetProgressObserver(progress);
		collsLALR.DoTransitions();

		if(debug)
			std::cout << "\n\nLALR(1):\n" << collsLALR << std::endl;
		if(write_graph)
			collsLALR.SaveGraph("script_lalr", 1);

		if(create_ascent_parser)
		{
			const char* parser_file = "script_parser.cpp";
			collsLALR.SaveParser(parser_file);

			std::cout << "Created recursive ascent parser \""
				<< parser_file << "\"." << std::endl;
		}

		if(create_tables)
		{
			const char* lalr_tables = "script.tab";
			collsLALR.SaveParseTables(lalr_tables);

			std::cout << "Created LALR(1) tables \""
				<< lalr_tables << "\"." << std::endl;
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

	// --------------------------------------------------------------------
	// get program arguments
	// --------------------------------------------------------------------
	bool create_asc = false;
	bool create_tables = false;
	bool debug = false;
	bool write_graph = false;
	bool show_help = false;

	args::options_description arg_descr("Script parser generator arguments");
	arg_descr.add_options()
		("asc,a", args::bool_switch(&create_asc), "create a recursive ascent parser")
		("table,t", args::bool_switch(&create_tables), "create LALR(1) tables")
		("graph,g", args::bool_switch(&write_graph), "write a graph of the parser")
		("debug,d", args::bool_switch(&debug), "enable debug output for parser generation")
		("help,h", args::bool_switch(&show_help), "show help");

	auto argparser = args::command_line_parser{argc, argv};
	argparser.style(args::command_line_style::default_style);
	argparser.options(arg_descr);

	args::variables_map mapArgs;
	auto parsedArgs = argparser.run();
	args::store(parsedArgs, mapArgs);
	args::notify(mapArgs);

	if(show_help)
	{
		std::cout << "Script parser generator"
			<< " by Tobias Weber <tobias.weber@tum.de>, 2022."
			<< std::endl;
		std::cout << "Internal data type lengths:"
			<< " real: " << sizeof(t_real)*8 << " bits,"
			<< " int: " << sizeof(t_int)*8 << " bits."
			<< std::endl;
		std::cout << "\n" << arg_descr << std::endl;
		return 0;
	}

	if(!create_asc && !create_tables)
	{
		std::cerr << "Warning: Neither recursive ascent parser"
			<< " nor LALR(1) table generation was selected,"
			<< " defaulting to recursive ascent parser."
			<< std::endl;
		create_asc = true;
	}
	// --------------------------------------------------------------------

	t_timepoint start_parsergen = t_clock::now();

	if(lr1_create_parser(create_asc, create_tables, debug, write_graph))
	{
		auto [run_time, time_unit] = get_elapsed_time<
			t_real, t_timepoint>(start_parsergen);
		std::cout << "Parser generation time: "
			<< run_time << " " << time_unit << "."
			<< std::endl;
	}

	return 0;
}