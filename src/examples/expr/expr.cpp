/**
 * expression parser example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 06-jun-2022
 * @license see 'LICENSE' file
 */

#include "core/collection.h"
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


#define DEBUG_CODEGEN     1
#define WRITE_BINFILE     0


#if __has_include("expr_parser.h")
	#include "expr_parser.h"
	#include "expr_parser.cpp"

	#define USE_RECASC 1

#elif __has_include("expr.tab")
	#include "core/parser.h"
	#include "expr.tab"

	#define USE_RECASC 0

#else
	static std::tuple<bool, std::string> lalr1_run_parser()
	{
		std::cerr << "No parsing tables available, please\n"
			"\t- run \"./expr_parsergen\" first,\n"
			"\t- \"touch " __FILE__ "\", and\n"
			"\t- rebuild using \"make\"."
			<< std::endl;

		return std::make_tuple(false, "");

	}

	#define __LALR_NO_PARSER_AVAILABLE
#endif


#ifndef __LALR_NO_PARSER_AVAILABLE

static void lalr1_run_parser()
{
	try
	{
		ExprGrammar grammar;
		grammar.CreateGrammar(false, true);
		const auto& rules = grammar.GetSemanticRules();

#if USE_RECASC != 0
		ExprParser parser;
#else
		// get created parsing tables
		auto [shift_tab, reduce_tab, jump_tab, num_rhs, lhs_idx] = get_lalr1_tables();
		auto [term_idx, nonterm_idx, semantic_idx] = get_lalr1_table_indices();
		auto [partials_rules_term, partials_matchlen_term,
			partials_rules_nonterm, partials_matchlen_nonterm]
				= get_lalr1_partials_tables();
		auto [err_idx, acc_idx, eps_id, end_id, start_idx] = get_lalr1_constants();

		Parser parser;
		parser.SetShiftTable(shift_tab);
		parser.SetReduceTable(reduce_tab);
		parser.SetJumpTable(jump_tab);
		parser.SetSemanticIdxMap(semantic_idx);
		parser.SetNumRhsSymsPerRule(num_rhs);
		parser.SetLhsIndices(lhs_idx);
		parser.SetPartialsRulesTerm(partials_rules_term);
		parser.SetPartialsMatchLenTerm(partials_matchlen_term);
		parser.SetPartialsRulesNonTerm(partials_rules_nonterm);
		parser.SetPartialsMatchLenNonTerm(partials_matchlen_nonterm);
		parser.SetEndId(end_id);
		parser.SetStartingState(start_idx);
#endif
		parser.SetSemanticRules(&rules);
		parser.SetDebug(true);

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
				if(tokid == static_cast<t_symbol_id>(Token::END))
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

#endif



int main()
{
	std::ios_base::sync_with_stdio(false);
	lalr1_run_parser();
	return 0;
}
