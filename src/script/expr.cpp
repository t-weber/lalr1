/**
 * expression test with operator precedences/associativities
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 06-jun-2022
 * @license see 'LICENSE' file
 */

#include "lalr1/collection.h"
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


enum : std::size_t
{
	START,
	EXPR,
};


// non-terminals
static NonTerminalPtr start, expr;

// terminals
static TerminalPtr op_plus, op_minus, op_mult, op_div, op_mod, op_pow;
static TerminalPtr bracket_open, bracket_close, comma;
static TerminalPtr sym_real, sym_int, ident;

// semantic rules
static std::vector<t_semanticrule> rules;

// indices from parse tables
static const t_mapIdIdx* mapNonTermIdx = nullptr;


static void create_grammar(bool add_semantics = true)
{
	start = std::make_shared<NonTerminal>(START, "start");
	expr = std::make_shared<NonTerminal>(EXPR, "expr");

	op_plus = std::make_shared<Terminal>('+', "+");
	op_minus = std::make_shared<Terminal>('-', "-");
	op_mult = std::make_shared<Terminal>('*', "*");
	op_div = std::make_shared<Terminal>('/', "/");
	op_mod = std::make_shared<Terminal>('%', "%");
	op_pow = std::make_shared<Terminal>('^', "^");
	bracket_open = std::make_shared<Terminal>('(', "(");
	bracket_close = std::make_shared<Terminal>(')', ")");
	comma = std::make_shared<Terminal>(',', ",");
	sym_real = std::make_shared<Terminal>((std::size_t)Token::REAL, "real");
	sym_int = std::make_shared<Terminal>((std::size_t)Token::INT, "integer");
	ident = std::make_shared<Terminal>((std::size_t)Token::IDENT, "ident");

	// precedences and associativities
	op_plus->SetPrecedence(70, 'l');
	op_minus->SetPrecedence(70, 'l');
	op_mult->SetPrecedence(80, 'l');
	op_div->SetPrecedence(80, 'l');
	op_mod->SetPrecedence(80, 'l');
	op_pow->SetPrecedence(110, 'r');


	// rule number
	std::size_t semanticindex = 0;

	// rule 0: start -> expr
	start->AddRule({ expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			return args[0];
		});
	}

	// rule 1: expr -> expr + expr
	expr->AddRule({ expr, op_plus, expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(id, tableidx, arg1, arg2, op_plus->GetId());
		});
	}


	// rule 2: expr -> expr - expr
	expr->AddRule({ expr, op_minus, expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(id, tableidx, arg1, arg2, op_minus->GetId());
		});
	}


	// rule 3: expr -> expr * expr
	expr->AddRule({ expr, op_mult, expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(id, tableidx, arg1, arg2, op_mult->GetId());
		});
	}


	// rule 4: expr -> expr / expr
	expr->AddRule({ expr, op_div, expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(id, tableidx, arg1, arg2, op_div->GetId());
		});
	}


	// rule 5: expr -> expr % expr
	expr->AddRule({ expr, op_mod, expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(id, tableidx, arg1, arg2, op_mod->GetId());
		});
	}


	// rule 6: expr -> expr ^ expr
	expr->AddRule({ expr, op_pow, expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(
				id, tableidx, arg1, arg2, op_pow->GetId());
		});
	}


	// rule 7: expr -> ( expr )
	expr->AddRule({ bracket_open, expr, bracket_close }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			return args[1];
		});
	}


	// function calls
	// rule 8: expr -> ident()
	expr->AddRule({ ident, bracket_open, bracket_close }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			std::size_t args_id = expr->GetId();
			std::size_t args_tableidx = mapNonTermIdx->find(args_id)->second;
			auto funcargs = std::make_shared<ASTList>(args_id, args_tableidx);

			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;
			return std::make_shared<ASTFuncCall>(id, tableidx, ident, funcargs);
		});
	}


	// rule 9: expr -> ident(expr)
	expr->AddRule({ ident, bracket_open, expr, bracket_close }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			std::size_t args_id = expr->GetId();
			std::size_t args_tableidx = mapNonTermIdx->find(args_id)->second;
			auto funcargs = std::make_shared<ASTList>(args_id, args_tableidx);

			t_astbaseptr funcarg = std::dynamic_pointer_cast<ASTBase>(args[2]);
			funcargs->AddChild(funcarg, false);

			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;
			return std::make_shared<ASTFuncCall>(id, tableidx, ident, funcargs);
		});
	}


	// rule 10: expr -> ident(expr, expr)
	expr->AddRule({ ident, bracket_open, expr, comma, expr, bracket_close }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			std::size_t args_id = expr->GetId();
			std::size_t args_tableidx = mapNonTermIdx->find(args_id)->second;

			t_astbaseptr funcarg1 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr funcarg2 = std::dynamic_pointer_cast<ASTBase>(args[4]);
			auto funcargs = std::make_shared<ASTList>(args_id, args_tableidx);
			funcargs->AddChild(funcarg2, false);
			funcargs->AddChild(funcarg1, false);

			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;
			return std::make_shared<ASTFuncCall>(id, tableidx, ident, funcargs);
		});
	}


	// rule 11: expr -> real symbol
	expr->AddRule({ sym_real }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr sym = std::dynamic_pointer_cast<ASTBase>(args[0]);
			sym->SetDataType(VMType::REAL);
			sym->SetId(id);
			sym->SetTableIdx(tableidx);

			return sym;
		});
	}


	// rule 12: expr -> int symbol
	expr->AddRule({ sym_int }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr sym = std::dynamic_pointer_cast<ASTBase>(args[0]);
			sym->SetDataType(VMType::INT);
			sym->SetId(id);
			sym->SetTableIdx(tableidx);

			return sym;
		});
	}


	// rule 13: expr -> ident
	expr->AddRule({ ident }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr ident = std::dynamic_pointer_cast<ASTBase>(args[0]);
			ident->SetDataType(VMType::INT);
			ident->SetId(id);
			ident->SetTableIdx(tableidx);

			return ident;
		});
	}


	// rule 14, unary-: expr -> -expr
	expr->AddRule({ op_minus, expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr arg = std::dynamic_pointer_cast<ASTBase>(args[1]);
			return std::make_shared<ASTUnary>(id, tableidx, arg, op_minus->GetId());
		});
	}


	// rule 15, unary+: expr -> +expr
	expr->AddRule({ op_plus, expr }, semanticindex++);
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			std::size_t id = expr->GetId();
			std::size_t tableidx = mapNonTermIdx->find(id)->second;

			t_astbaseptr arg = std::dynamic_pointer_cast<ASTBase>(args[1]);
			return std::make_shared<ASTUnary>(id, tableidx, arg, op_plus->GetId());
		});
	}
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
			calc_first(nonterminal, first, &first_per_rule);

		for(const auto& pair : first)
		{
			std::cout << pair.first->GetStrId() << ": ";
			for(const auto& _first : pair.second)
				std::cout << _first->GetStrId() << ", ";
			std::cout << "\n";
		}
		std::cout << std::endl;

		std::cout << "FOLLOW sets:\n";
		t_map_follow follow;
		for(const NonTerminalPtr& nonterminal : all_nonterminals)
			calc_follow(all_nonterminals, start, nonterminal, first, follow);

		for(const auto& pair : follow)
		{
			std::cout << pair.first->GetStrId() << ": ";
			for(const auto& _first : pair.second)
				std::cout << _first->GetStrId() << ", ";
			std::cout << "\n";
		}
		std::cout << std::endl;
#endif


		ElementPtr elem = std::make_shared<Element>(
			start, 0, 0, Terminal::t_terminalset{{g_end}});
		ClosurePtr closure = std::make_shared<Closure>();
		closure->AddElement(elem);
		//std::cout << "\n\n" << *closure << std::endl;

		auto progress = [](const std::string& msg, [[maybe_unused]] bool done)
		{
			std::cout << "\r" << msg << "                ";
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
		collsLALR.WriteGraph("expr", 1);
#endif

		auto parsetables = collsLALR.CreateParseTables();
		collsLALR.SaveParseTables(parsetables, "expr.tab");
	}
	catch(const std::exception& err)
	{
		std::cerr << "Error: " << err.what() << std::endl;
	}
}

#endif



#ifdef RUN_PARSER

#if !__has_include("expr.tab")

static void lalr1_run_parser()
{
	std::cerr
		<< "No parsing tables available, please run ./expr_create first and rebuild."
		<< std::endl;
}

#else

#include "lalr1/parser.h"
#include "expr.tab"

static void lalr1_run_parser()
{
	try
	{
		// get created parsing tables
		auto parsetables = get_lr1_tables();

		const t_mapIdIdx* mapTermIdx = std::get<3>(parsetables);
		mapNonTermIdx = std::get<4>(parsetables);


		Parser parser{parsetables, rules};

		while(1)
		{
			std::string exprstr;
			std::cout << "\nExpression: ";
			std::getline(std::cin, exprstr);
			std::istringstream istr{exprstr};

			auto tokens = get_all_tokens(istr, mapTermIdx);

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
