/**
 * expression parser generator example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 06-jun-2022
 * @license see 'LICENSE' file
 */

#include "core/collection.h"
#include "core/tablegen.h"
#include "core/parsergen.h"
#include "core/options.h"
#include "expr_grammar.h"
#include "script/lexer.h"
#include "script/ast.h"
#include "script/ast_printer.h"
#include "script/ast_asm.h"
#include "script_vm/vm.h"

#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstdint>


using namespace lalr1;


#define DEBUG_PARSERGEN   1
#define USE_RECASC        0


static void lr1_create_parser()
{
	try
	{
		ExprGrammar grammar;
		grammar.CreateGrammar(true, false);
		NonTerminalPtr start = grammar.GetStartNonTerminal();

#if DEBUG_PARSERGEN != 0
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
#endif

		ElementPtr elem = std::make_shared<Element>(
			start, 0, 0, Terminal::t_terminalset{{ g_end }});
		ClosurePtr closure = std::make_shared<Closure>();
		closure->AddElement(elem);
		//std::cout << "\n\n" << *closure << std::endl;

		auto progress = [](const std::string& msg, [[maybe_unused]] bool done)
		{
			std::cout << "\r\x1b[K" << msg;
			if(done)
				std::cout << "\n";
			std::cout.flush();
		};

		CollectionPtr collsLALR = std::make_shared<Collection>(closure);
		//collsLALR->SetSolveReduceConflicts(false);
		//collsLALR->SetStopOnConflicts(false);
		//collsLALR->SetDontGenerateLookbacks(true);
		collsLALR->SetProgressObserver(progress);
		collsLALR->DoTransitions();

#if DEBUG_PARSERGEN != 0
		std::cout << "\n\n" << (*collsLALR) << std::endl;
		collsLALR->SaveGraph("expr", 1);
#endif

#if USE_RECASC != 0
		ParserGen parsergen{collsLALR};
		parsergen.SetAcceptingRule(static_cast<t_semantic_id>(Semantics::START));
		parsergen.SetUseStateNames(true);
		parsergen.SaveParser("expr_parser.cpp", "ExprParser");
#else
		bool tables_ok = false;
		TableGen exporter{collsLALR};
		exporter.SetAcceptingRule(static_cast<t_semantic_id>(Semantics::START));

		if(exporter.CreateParseTables())
		{
			tables_ok = exporter.SaveParseTablesCXX("expr.tab");
			exporter.SaveParseTablesJSON("expr.json");
			exporter.SaveParseTablesJava("ExprTab.java");
			exporter.SaveParseTablesRS("expr.rs");
		}

		if(!tables_ok)
			std::cerr << "Error: Parsing tables could not be created." << std::endl;
#endif
	}
	catch(const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
	}
}



int main()
{
	std::ios_base::sync_with_stdio(false);

	g_options.SetUseColour(true);
	lr1_create_parser();
	return 0;
}
