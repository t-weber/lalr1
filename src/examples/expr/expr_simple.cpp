/**
 * simplified expression test
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-aug-2022
 * @license see 'LICENSE' file
 */

#include "lalr1/collection.h"
#include "lalr1/tableexport.h"
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


#define DEBUG_PARSERGEN   1
#define DEBUG_WRITEGRAPH  1
#define DEBUG_CODEGEN     1
#define WRITE_BINFILE     0


enum : std::size_t
{
	START,
	EXPR,
};


// non-terminals
static NonTerminalPtr start, expr;

// terminals
static TerminalPtr op_plus, op_mult;
static TerminalPtr sym_real;

// semantic rules
static t_semanticrules rules;


static void create_grammar(bool add_semantics = true)
{
	start = std::make_shared<NonTerminal>(START, "start");
	expr = std::make_shared<NonTerminal>(EXPR, "expr");

	op_plus = std::make_shared<Terminal>('+', "+");
	op_mult = std::make_shared<Terminal>('*', "*");
	sym_real = std::make_shared<Terminal>((std::size_t)Token::REAL, "real");

	// precedences and associativities
	op_plus->SetPrecedence(70, 'l');
	op_mult->SetPrecedence(80, 'l');


	// rule number
	t_semantic_id semanticindex = 0;

	// rule 0: start -> expr
	start->AddRule({ expr }, semanticindex);
	if(add_semantics)
	{
		rules.emplace(std::make_pair(semanticindex,
		[](bool full_match, const t_semanticargs& args) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;
			return args[0];
		}));
	}
	++semanticindex;

	// rule 1: expr -> expr + expr
	expr->AddRule({ expr, op_plus, expr }, semanticindex);
	if(add_semantics)
	{
		rules.emplace(std::make_pair(semanticindex,
		[](bool full_match, const t_semanticargs& args) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_plus->GetId());
		}));
	}
	++semanticindex;

	// rule 2: expr -> expr * expr
	expr->AddRule({ expr, op_mult, expr }, semanticindex);
	if(add_semantics)
	{
		rules.emplace(std::make_pair(semanticindex,
		[](bool full_match, const t_semanticargs& args) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_mult->GetId());
		}));
	}
	++semanticindex;

	// rule 3: expr -> real symbol
	expr->AddRule({ sym_real }, semanticindex);
	if(add_semantics)
	{
		rules.emplace(std::make_pair(semanticindex,
		[](bool full_match, const t_semanticargs& args) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			std::size_t id = expr->GetId();
			t_astbaseptr sym = std::dynamic_pointer_cast<ASTBase>(args[0]);
			sym->SetDataType(VMType::REAL);
			sym->SetId(id);
			return sym;
		}));
	}
	++semanticindex;
}



#ifdef CREATE_PARSER

static void lr1_create_parser()
{
	try
	{
#if DEBUG_PARSERGEN != 0
		std::vector<NonTerminalPtr> all_nonterminals{{ start, expr }};

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
			start, 0, 0, Terminal::t_terminalset{{g_end}});
		ClosurePtr closure = std::make_shared<Closure>();
		closure->AddElement(elem);

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
		std::cout << "\n\n" << collsLALR << std::endl;
#endif

#if DEBUG_WRITEGRAPH != 0
		collsLALR.SaveGraph("expr_simple", 1);
#endif

		if(collsLALR.CreateParseTables())
		{
			TableExporter exporter{&collsLALR};
			exporter.SaveParseTablesCXX("expr_simple.tab");
		}
		collsLALR.SaveParser("expr_simple_parser.cpp");
	}
	catch(const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
	}
}

#endif



#ifdef RUN_PARSER

#if !__has_include("expr_simple.tab")

static void lalr1_run_parser()
{
	std::cerr
		<< "No parsing tables available, please run ./expr_simple_parsergen first and rebuild."
		<< std::endl;
}

#else

#include "lalr1/parser.h"
#include "expr_simple.tab"

static void lalr1_run_parser()
{
	try
	{
		// get created parsing tables
		auto [shift_tab, reduce_tab, jump_tab, num_rhs, lhs_idx] = get_lalr1_tables();
		auto [term_idx, nonterm_idx, semantic_idx] = get_lalr1_table_indices();
		auto [err_idx, acc_idx, eps_id, end_id] = get_lalr1_constants();

		Parser parser;
		parser.SetShiftTable(shift_tab);
		parser.SetReduceTable(reduce_tab);
		parser.SetJumpTable(jump_tab);
		parser.SetSemanticIdxMap(semantic_idx);
		parser.SetNumRhsSymsPerRule(num_rhs);
		parser.SetLhsIndices(lhs_idx);
		parser.SetSemanticRules(&rules);
		parser.SetEndId(end_id);
		parser.SetDebug(true);

		while(1)
		{
			std::string exprstr;
			std::cout << "\nExpression: ";
			std::getline(std::cin, exprstr);
			std::istringstream istr{exprstr};

			Lexer lexer(&istr);
			lexer.SetIgnoreInt(true);
			lexer.SetTermIdxMap(term_idx);
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
				std::make_pair('*', std::make_tuple("mul", OpCode::MUL)),
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
			std::string binfile{"expr_simple.bin"};
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
#endif



int main()
{
	std::ios_base::sync_with_stdio(false);

#ifdef CREATE_PARSER
	create_grammar(false);
	lr1_create_parser();
#endif

#ifdef RUN_PARSER
	create_grammar(true);
	lalr1_run_parser();
#endif

	return 0;
}
