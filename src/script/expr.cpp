/**
 * expression parser example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 06-jun-2022
 * @license see 'LICENSE' file
 */

#include "lalr1/collection.h"
#include "expr_grammar.h"
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


#define DEBUG_PARSERGEN   1
#define DEBUG_WRITEGRAPH  0
#define DEBUG_CODEGEN     1
#define WRITE_BINFILE     0
#define USE_RECASC        0


#ifdef CREATE_PARSER

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

		Collection collsLALR{ closure };
		collsLALR.SetProgressObserver(progress);
		collsLALR.DoTransitions();

#if DEBUG_PARSERGEN != 0
		std::cout << "\n\nLALR(1):\n" << collsLALR << std::endl;
#endif

#if DEBUG_WRITEGRAPH != 0
		collsLALR.SaveGraph("expr", 1);
#endif

#if USE_RECASC != 0
	collsLALR.SaveParser("expr_parser.cpp");
#else
	collsLALR.SaveParseTables("expr.tab");
#endif
	}
	catch(const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
	}
}

#endif  // CREATE_PARSER



#ifdef RUN_PARSER

#if USE_RECASC != 0
	#if __has_include("expr_parser.h")
		#include "expr_parser.h"
		#include "expr_parser.cpp"

		#define __LALR1_CANRUN_PARSER
	#endif
#else
	#include "lalr1/parser.h"
	#if __has_include("expr.tab")
		#include "expr.tab"

		#define __LALR1_CANRUN_PARSER
	#endif
#endif


#ifdef __LALR1_CANRUN_PARSER

static void lalr1_run_parser()
{
	try
	{
		ExprGrammar grammar;
		grammar.CreateGrammar(false, true);
		const auto& rules = grammar.GetSemanticRules();

#if USE_RECASC != 0
		ParserRecAsc parser;
#else
		// get created parsing tables
		auto [shift_tab, reduce_tab, jump_tab, num_rhs, lhs_idx] = get_lalr1_tables();
		auto [term_idx, nonterm_idx] = get_lalr1_table_indices();

		Parser parser;
		parser.SetShiftTable(shift_tab);
		parser.SetReduceTable(reduce_tab);
		parser.SetJumpTable(jump_tab);
		parser.SetNumRhsSymsPerRule(num_rhs);
		parser.SetLhsIndices(lhs_idx);
#endif
		parser.SetSemanticRules(&rules);
		//parser.SetDebug(true);

		while(1)
		{
			std::string exprstr;
			std::cout << "\nExpression: ";
			std::getline(std::cin, exprstr);
			std::istringstream istr{exprstr};

			Lexer lexer(&istr);
#if USE_RECASC == 0
			lexer.SetTermIdxMap(term_idx);
#endif
			auto tokens = lexer.GetAllTokens();

#if DEBUG_CODEGEN != 0
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
			std::cout << "\n";
#endif

			t_astbaseptr ast = std::dynamic_pointer_cast<ASTBase>(parser.Parse(tokens));
			ast->AssignLineNumbers();
			ast->DeriveDataType();

#if DEBUG_CODEGEN != 0
			std::cout << "\nAST:\n";
			ASTPrinter printer{std::cout};
			ast->accept(&printer);
#endif

			std::unordered_map<std::size_t, std::tuple<std::string, OpCode>> ops
			{{
				std::make_pair('+', std::make_tuple("add", OpCode::ADD)),
				std::make_pair('-', std::make_tuple("sub", OpCode::SUB)),
				std::make_pair('*', std::make_tuple("mul", OpCode::MUL)),
				std::make_pair('/', std::make_tuple("div", OpCode::DIV)),
				std::make_pair('%', std::make_tuple("mod", OpCode::MOD)),
				std::make_pair('^', std::make_tuple("pow", OpCode::POW)),
			}};

#if DEBUG_CODEGEN != 0
			std::ostringstream ostrAsm;
			ASTAsm astasm{ostrAsm, &ops};
			astasm.AlwaysCallExternal(true);
			ast->accept(&astasm);
#endif

			std::ostringstream ostrAsmBin(
				std::ios_base::out | std::ios_base::binary);
			ASTAsm astasmbin{ostrAsmBin, &ops};
			astasmbin.AlwaysCallExternal(true);
			astasmbin.SetBinary(true);
			ast->accept(&astasmbin);
			astasmbin.FinishCodegen();
			std::string strAsmBin = ostrAsmBin.str();

#if WRITE_BINFILE != 0
			std::string binfile{"expr.bin"};
			std::ofstream ofstrAsmBin(binfile, std::ios_base::binary);
			if(!ofstrAsmBin)
			{
				std::cerr << "Cannot open \""
					<< binfile << "\"." << std::endl;
			}
			ofstrAsmBin.write(strAsmBin.data(), strAsmBin.size());
			if(ofstrAsmBin.fail())
			{
				std::cerr << "Cannot write \""
					<< binfile << "\"." << std::endl;
			}
#endif

#if DEBUG_CODEGEN != 0
			std::cout << "\nGenerated code ("
				<< strAsmBin.size() << " bytes):\n"
				<< ostrAsm.str();
#endif

			VM vm(1024);
			//vm.SetDebug(true);
			vm.SetMem(0, strAsmBin);
			vm.Run();

			std::cout << "\nResult: ";
			std::visit([](auto&& val) -> void
			{
				using t_val = std::decay_t<decltype(val)>;
				if constexpr(!std::is_same_v<t_val, std::monostate>)
					std::cout << val;
			}, vm.TopData());
			std::cout << std::endl;
		}
	}
	catch(const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
	}
}

#else

static void lalr1_run_parser()
{
	std::cerr << "No parsing tables available, please run ./expr_parsergen first and rebuild."
		<< std::endl;
}

#endif
#endif  // RUN_PARSER



int main()
{
	std::ios_base::sync_with_stdio(false);

#ifdef CREATE_PARSER
	lr1_create_parser();
#endif

#ifdef RUN_PARSER
	lalr1_run_parser();
#endif

	return 0;
}
