/**
 * differentiating expression grammar example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 16-jul-2023
 * @license see 'LICENSE' file
 */

#include "grammar.h"
#include "script/lexer.h"


using t_lalrastbaseptr = lalr1::t_astbaseptr;
using lalr1::Terminal;
using lalr1::NonTerminal;
using lalr1::t_semanticargs;
using lalr1::g_eps;


void DiffGrammar::CreateGrammar(bool add_rules, bool add_semantics)
{
	// non-terminals
	start = std::make_shared<NonTerminal>(START, "start");
	expr = std::make_shared<NonTerminal>(EXPR, "expr");

	// terminals
	op_plus = std::make_shared<Terminal>('+', "+");
	op_minus = std::make_shared<Terminal>('-', "-");
	op_mult = std::make_shared<Terminal>('*', "*");
	op_div = std::make_shared<Terminal>('/', "/");
	op_mod = std::make_shared<Terminal>('%', "%");
	op_pow = std::make_shared<Terminal>('^', "^");
	bracket_open = std::make_shared<Terminal>('(', "(");
	bracket_close = std::make_shared<Terminal>(')', ")");
	comma = std::make_shared<Terminal>(',', ",");
	sym_real = std::make_shared<Terminal>(static_cast<t_symbol_id>(Token::REAL), "real");
	sym_int = std::make_shared<Terminal>(static_cast<t_symbol_id>(Token::INT), "integer");
	ident = std::make_shared<Terminal>(static_cast<t_symbol_id>(Token::IDENT), "ident");

	// precedences and associativities
	op_plus->SetPrecedence(70, 'l');
	op_minus->SetPrecedence(70, 'l');
	op_mult->SetPrecedence(80, 'l');
	op_div->SetPrecedence(80, 'l');
	op_mod->SetPrecedence(80, 'l');
	op_pow->SetPrecedence(110, 'r');


	// rule 0: start -> expr
	if(add_rules)
	{
		start->AddRule({ expr }, static_cast<t_semantic_id>(Semantics::START));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::START),
		[](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;
			return args[0];
		}));
	}


	// rule 1: expr -> expr + expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_plus, expr }, static_cast<t_semantic_id>(Semantics::ADD));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::ADD),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr diffarg1 = std::dynamic_pointer_cast<ASTBase>(arg1->GetSubAST(0));
			t_astbaseptr diffarg2 = std::dynamic_pointer_cast<ASTBase>(arg2->GetSubAST(0));

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_plus->GetId());
			auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0, diffarg1, diffarg2, op_plus->GetId());
			newast->AddSubAST(diffast);

			return newast;
		}));
	}


	// rule 2: expr -> expr - expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_minus, expr }, static_cast<t_semantic_id>(Semantics::SUB));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::SUB),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr diffarg1 = std::dynamic_pointer_cast<ASTBase>(arg1->GetSubAST(0));
			t_astbaseptr diffarg2 = std::dynamic_pointer_cast<ASTBase>(arg2->GetSubAST(0));

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_minus->GetId());
			auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0, diffarg1, diffarg2, op_minus->GetId());
			newast->AddSubAST(diffast);

			return newast;
		}));
	}


	// rule 3: expr -> expr * expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_mult, expr },
			static_cast<t_semantic_id>(Semantics::MUL));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::MUL),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr diffarg1 = std::dynamic_pointer_cast<ASTBase>(arg1->GetSubAST(0));
			t_astbaseptr diffarg2 = std::dynamic_pointer_cast<ASTBase>(arg2->GetSubAST(0));

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_mult->GetId());
			auto diffast1 = std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, diffarg2, op_mult->GetId());
			auto diffast2 = std::make_shared<ASTBinary>(expr->GetId(), 0, arg2, diffarg1, op_mult->GetId());
			auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0, diffast1, diffast2, op_plus->GetId());
			newast->AddSubAST(diffast);

			return newast;
		}));
	}


	// rule 4: expr -> expr / expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_div, expr }, static_cast<t_semantic_id>(Semantics::DIV));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::DIV),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr diffarg1 = std::dynamic_pointer_cast<ASTBase>(arg1->GetSubAST(0));
			t_astbaseptr diffarg2 = std::dynamic_pointer_cast<ASTBase>(arg2->GetSubAST(0));

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_div->GetId());
			auto diffast1 = std::make_shared<ASTBinary>(expr->GetId(), 0, diffarg1, arg2, op_div->GetId());
			auto diffast2a = std::make_shared<ASTBinary>(expr->GetId(), 0, diffarg2, arg1, op_mult->GetId());
			auto diffast2b = std::make_shared<ASTBinary>(expr->GetId(), 0, arg2, arg2, op_mult->GetId());
			auto diffast2 = std::make_shared<ASTBinary>(expr->GetId(), 0, diffast2a, diffast2b, op_div->GetId());
			auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0, diffast1, diffast2, op_minus->GetId());
			newast->AddSubAST(diffast);

			return newast;
		}));
	}


	// rule 5: expr -> expr % expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_mod, expr }, static_cast<t_semantic_id>(Semantics::MOD));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::MOD),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr diffarg1 = std::dynamic_pointer_cast<ASTBase>(arg1->GetSubAST(0));
			t_astbaseptr diffarg2 = std::dynamic_pointer_cast<ASTBase>(arg2->GetSubAST(0));

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_mod->GetId());
			// TODO: diffast
			return newast;
		}));
	}


	// rule 6: expr -> expr ^ expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_pow, expr }, static_cast<t_semantic_id>(Semantics::POW));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::POW),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr diffarg1 = std::dynamic_pointer_cast<ASTBase>(arg1->GetSubAST(0));
			t_astbaseptr diffarg2 = std::dynamic_pointer_cast<ASTBase>(arg2->GetSubAST(0));

			auto newast = std::make_shared<ASTBinary>(
				expr->GetId(), 0, arg1, arg2, op_pow->GetId());
			// TODO: diffast
			return newast;
		}));
	}


	// rule 7: expr -> ( expr )
	if(add_rules)
	{
		expr->AddRule({ bracket_open, expr, bracket_close }, static_cast<t_semantic_id>(Semantics::BRACKETS));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::BRACKETS),
		[](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;
			return args[1];
		}));
	}


	// function calls
	// rule 8: expr -> ident()
	if(add_rules)
	{
		expr->AddRule({ ident, bracket_open, bracket_close }, static_cast<t_semantic_id>(Semantics::CALL0));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::CALL0),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);

			return std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
		}));
	}


	// rule 9: expr -> ident(expr)
	if(add_rules)
	{
		expr->AddRule({ ident, bracket_open, expr, bracket_close },
			static_cast<t_semantic_id>(Semantics::CALL1));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::CALL1),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
			t_astbaseptr funcarg = std::dynamic_pointer_cast<ASTBase>(args[2]);
			funcargs->AddChild(funcarg, false);

			return std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
		}));
	}


	// rule 10: expr -> ident(expr, expr)
	if(add_rules)
	{
		expr->AddRule({ ident, bracket_open, expr, comma, expr, bracket_close },
			static_cast<t_semantic_id>(Semantics::CALL2));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::CALL2),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			t_astbaseptr funcarg1 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr funcarg2 = std::dynamic_pointer_cast<ASTBase>(args[4]);
			auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
			funcargs->AddChild(funcarg2, false);
			funcargs->AddChild(funcarg1, false);

			return std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
		}));
	}


	// rule 11: expr -> real symbol
	if(add_rules)
	{
		expr->AddRule({ sym_real }, static_cast<t_semantic_id>(Semantics::REAL));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::REAL),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr sym = std::dynamic_pointer_cast<ASTBase>(args[0]);
			sym->SetDataType(VMType::REAL);
			sym->SetId(expr->GetId());
			sym->SetTerminalOverride(false);  // expression, no terminal any more

			auto diffsym = std::make_shared<ASTToken<t_real>>(
				expr->GetId(), sym->GetTableIndex(),
				t_real(0), sym->GetLineRange()->first);
			diffsym->SetDataType(VMType::REAL);
			diffsym->SetTerminalOverride(false);
			sym->AddSubAST(diffsym);

			return sym;
		}));
	}


	// rule 12: expr -> int symbol
	if(add_rules)
	{
		expr->AddRule({ sym_int }, static_cast<t_semantic_id>(Semantics::INT));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::INT),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr sym = std::dynamic_pointer_cast<ASTBase>(args[0]);
			sym->SetDataType(VMType::INT);
			sym->SetId(expr->GetId());
			sym->SetTerminalOverride(false);  // expression, no terminal any more

			auto diffsym = std::make_shared<ASTToken<t_int>>(
				expr->GetId(), sym->GetTableIndex(),
				t_int(0), sym->GetLineRange()->first);
			diffsym->SetDataType(VMType::INT);
			diffsym->SetTerminalOverride(false);
			sym->AddSubAST(diffsym);

			return sym;
		}));
	}


	// rule 13: expr -> ident
	if(add_rules)
	{
		expr->AddRule({ ident }, static_cast<t_semantic_id>(Semantics::IDENT));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::IDENT),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			auto ident = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			ident->SetIdent(true);
			//ident->SetDataType(VMType::INT);
			ident->SetId(expr->GetId());
			ident->SetTerminalOverride(false);  // expression, no terminal any more

			const std::string& varname = ident->GetLexerValue();
			std::string diffvarname = "x";  // TODO: make configurable

			auto diffident = std::make_shared<ASTToken<t_int>>(
				expr->GetId(), ident->GetTableIndex(),
				t_int(varname == diffvarname ? 1 : 0),
				ident->GetLineRange()->first);
			diffident->SetDataType(VMType::INT);
			diffident->SetTerminalOverride(false);
			ident->AddSubAST(diffident);

			return ident;
		}));
	}


	// rule 14, unary-: expr -> -expr
	if(add_rules)
	{
		expr->AddRule({ op_minus, expr }, static_cast<t_semantic_id>(Semantics::USUB));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::USUB),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg = std::dynamic_pointer_cast<ASTBase>(args[1]);
			t_astbaseptr diffarg = std::dynamic_pointer_cast<ASTBase>(arg->GetSubAST(0));

			auto newast = std::make_shared<ASTUnary>(expr->GetId(), 0, arg, op_minus->GetId());
			auto diffast = std::make_shared<ASTUnary>(expr->GetId(), 0, diffarg, op_minus->GetId());
			newast->AddSubAST(diffast);

			return newast;
		}));
	}


	// rule 15, unary+: expr -> +expr
	if(add_rules)
	{
		expr->AddRule({ op_plus, expr }, static_cast<t_semantic_id>(Semantics::UADD));
	}
	if(add_semantics)
	{
		rules.emplace(std::make_pair(static_cast<t_semantic_id>(Semantics::UADD),
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;

			t_astbaseptr arg = std::dynamic_pointer_cast<ASTBase>(args[1]);
			t_astbaseptr diffarg = std::dynamic_pointer_cast<ASTBase>(arg->GetSubAST(0));

			auto newast = std::make_shared<ASTUnary>(expr->GetId(), 0, arg, op_plus->GetId());
			auto diffast = std::make_shared<ASTUnary>(expr->GetId(), 0, diffarg, op_plus->GetId());
			newast->AddSubAST(diffast);

			return newast;
		}));
	}
}