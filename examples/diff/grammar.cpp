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



/**
 * create the differential of a function with 0 arguments
 */
t_astbaseptr DiffGrammar::MakeDiffFunc0([[__maybe_unused__]] const std::string& ident) const
{
	// always 0 because it doesn't depend on any variable
	return this->CreateIntConst(0);
}


/**
 * create the differential of a function with 1 argument
 */
t_astbaseptr DiffGrammar::MakeDiffFunc1(const std::string& ident, const t_astbaseptr& arg) const
{
	// df(x) / dx
	t_astbaseptr diffarg = std::dynamic_pointer_cast<ASTBase>(arg->GetSubAST(0));

	if(ident == "exp")
	{
		// exp(f(x))
		auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
		funcargs->AddChild(arg, false);
		auto diffast_func = std::make_shared<ASTFuncCall>(
			expr->GetId(), 0, "exp", funcargs);

		// exp(f(x)) * df(x)/dx
		auto diffast_mult = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffarg, diffast_func, op_mult->GetId());
		return diffast_mult;
	}
	else if(ident == "log")
	{
		// df(x) / (dx * f(x))
		auto diffast_div = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffarg, arg, op_div->GetId());
		return diffast_div;
	}
	else if(ident == "sin")
	{
		// cos(f(x))
		auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
		funcargs->AddChild(arg, false);
		auto diffast_func = std::make_shared<ASTFuncCall>(
			expr->GetId(), 0, "cos", funcargs);

		// cos(f(x)) * df(x)/dx
		auto diffast_mult = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffarg, diffast_func, op_mult->GetId());
		return diffast_mult;
	}
	else if(ident == "cos")
	{
		// sin(f(x))
		auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
		funcargs->AddChild(arg, false);
		auto diffast_func = std::make_shared<ASTFuncCall>(
			expr->GetId(), 0, "sin", funcargs);

		// -sin(f(x))
		auto diffast_umin = std::make_shared<ASTUnary>(expr->GetId(), 0,
			diffast_func, op_minus->GetId());

		// -sin(f(x)) * df(x)/dx
		auto diffast_mult = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffarg, diffast_umin, op_mult->GetId());
		return diffast_mult;
	}
	else if(ident == "tan")
	{
		// cos(f(x))
		auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
		funcargs->AddChild(arg, false);
		auto diffast_func = std::make_shared<ASTFuncCall>(
			expr->GetId(), 0, "cos", funcargs);

		// 1 / cos^2(f(x))
		auto minus2 = this->CreateIntConst(-2);
		auto diffast_pow = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffast_func, minus2, op_pow->GetId());

		// 1 / cos^2(f(x)) * df(x) / dx
		auto diffast_mult = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffarg, diffast_pow, op_mult->GetId());
		return diffast_mult;
	}
	else if(ident == "asin")
	{
		auto one = this->CreateRealConst(1.);
		one->SetLineRange(arg->GetLineRange());

		// f(x)^2
		auto diffast_square = std::make_shared<ASTBinary>(expr->GetId(), 0,
			arg, arg, op_mult->GetId());
		// 1 - f(x)^2
		auto diffast_minus = std::make_shared<ASTBinary>(expr->GetId(), 0,
			one, diffast_square, op_minus->GetId());

		// sqrt(1 - f(x)^2)
		auto sqrtargargs = std::make_shared<ASTList>(expr->GetId(), 0);
		sqrtargargs->AddChild(diffast_minus, false);
		auto diffast_sqrt = std::make_shared<ASTFuncCall>(
			expr->GetId(), 0, "sqrt", sqrtargargs);

		// 1 / sqrt(1 - f(x)^2)
		auto diffast_div = std::make_shared<ASTBinary>(expr->GetId(), 0,
			one, diffast_sqrt, op_div->GetId());

		// 1 / sqrt(1 - f(x)^2) * df(x)/dx)
		auto diffast_mult = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffarg, diffast_div, op_mult->GetId());
		return diffast_mult;
	}
	else if(ident == "acos")
	{
		auto one = this->CreateRealConst(1.);
		auto minus_one = this->CreateRealConst(-1.);
		one->SetLineRange(arg->GetLineRange());
		minus_one->SetLineRange(arg->GetLineRange());

		// f(x)^2
		auto diffast_square = std::make_shared<ASTBinary>(expr->GetId(), 0,
			arg, arg, op_mult->GetId());
		// 1 - f(x)^2
		auto diffast_minus = std::make_shared<ASTBinary>(expr->GetId(), 0,
			one, diffast_square, op_minus->GetId());

		// sqrt(1 - f(x)^2)
		auto sqrtargargs = std::make_shared<ASTList>(expr->GetId(), 0);
		sqrtargargs->AddChild(diffast_minus, false);
		auto diffast_sqrt = std::make_shared<ASTFuncCall>(
			expr->GetId(), 0, "sqrt", sqrtargargs);

		// -1 / sqrt(1 - f(x)^2)
		auto diffast_div = std::make_shared<ASTBinary>(expr->GetId(), 0,
			minus_one, diffast_sqrt, op_div->GetId());

		// -1 / sqrt(1 - f(x)^2) * df(x)/dx)
		auto diffast_mult = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffarg, diffast_div, op_mult->GetId());
		return diffast_mult;
	}
	else if(ident == "atan")
	{
		auto one = this->CreateRealConst(1.);
		one->SetLineRange(arg->GetLineRange());

		// f(x)^2
		auto diffast_square = std::make_shared<ASTBinary>(expr->GetId(), 0,
			arg, arg, op_mult->GetId());
		// 1 + f(x)^2
		auto diffast_plus = std::make_shared<ASTBinary>(expr->GetId(), 0,
			one, diffast_square, op_plus->GetId());

		// 1 / (1 + f(x)^2)
		auto diffast_div = std::make_shared<ASTBinary>(expr->GetId(), 0,
			one, diffast_plus, op_div->GetId());

		// 1 / (1 + f(x)^2) * df(x)/dx)
		auto diffast_mult = std::make_shared<ASTBinary>(expr->GetId(), 0,
			diffarg, diffast_div, op_mult->GetId());
		return diffast_mult;
	}
	if(ident == "sqrt")
	{
		auto half = this->CreateRealConst(0.5);
		half->SetLineRange(arg->GetLineRange());

		auto zero = this->CreateRealConst(0);
		zero->SetLineRange(arg->GetLineRange());
		half->AddSubAST(zero);

		return MakePowFunc(arg, half, true);
	}
	else
	{
		// return 0 for unknown functions
		return this->CreateIntConst(0);
	}

	return nullptr;
}


/**
 * create the differential of a function with 2 arguments
 */
t_astbaseptr DiffGrammar::MakeDiffFunc2(const std::string& ident,
	const t_astbaseptr& arg1, const t_astbaseptr& arg2) const
{
	//t_astbaseptr diffarg1 = std::dynamic_pointer_cast<ASTBase>(arg1->GetSubAST(0));
	//t_astbaseptr diffarg2 = std::dynamic_pointer_cast<ASTBase>(arg2->GetSubAST(0));

	if(ident == "pow")
	{
		return MakePowFunc(arg1, arg2, true);
	}
	else
	{
		// return 0 for unknown functions
		return CreateIntConst(0);
	}

	return nullptr;
}


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
		[this](bool full_match, const t_semanticargs& args, [[maybe_unused]] t_lalrastbaseptr retval) -> t_lalrastbaseptr
		{
			if(!full_match) return nullptr;
			//return args[0];

			// add a dummy unary node to allow optimisation on the root node
			t_astbaseptr arg = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr diffarg = std::dynamic_pointer_cast<ASTBase>(arg->GetSubAST(0));

			auto newast = std::make_shared<ASTUnary>(expr->GetId(), 0,
				arg, op_plus->GetId());
			auto diffast = std::make_shared<ASTUnary>(expr->GetId(), 0,
				diffarg, op_plus->GetId());
			newast->AddSubAST(diffast);
			return newast;
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

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				arg1, arg2, op_plus->GetId());
			auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				diffarg1, diffarg2, op_plus->GetId());
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

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				arg1, arg2, op_minus->GetId());
			auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				diffarg1, diffarg2, op_minus->GetId());
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

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				arg1, arg2, op_mult->GetId());
			auto diffast1 = std::make_shared<ASTBinary>(expr->GetId(), 0,
				arg1, diffarg2, op_mult->GetId());
			auto diffast2 = std::make_shared<ASTBinary>(expr->GetId(), 0,
				arg2, diffarg1, op_mult->GetId());
			auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				diffast1, diffast2, op_plus->GetId());
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

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				arg1, arg2, op_div->GetId());
			auto diffast1 = std::make_shared<ASTBinary>(expr->GetId(), 0,
				diffarg1, arg2, op_div->GetId());
			auto diffast2a = std::make_shared<ASTBinary>(expr->GetId(), 0,
				diffarg2, arg1, op_mult->GetId());
			auto diffast2b = std::make_shared<ASTBinary>(expr->GetId(), 0,
				arg2, arg2, op_mult->GetId());
			auto diffast2 = std::make_shared<ASTBinary>(expr->GetId(), 0,
				diffast2a, diffast2b, op_div->GetId());
			auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				diffast1, diffast2, op_minus->GetId());
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

			auto newast = std::make_shared<ASTBinary>(expr->GetId(), 0,
				arg1, arg2, op_mod->GetId());
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

			return MakePowFunc(arg1, arg2, false);
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

			auto newast = std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
			newast->AddSubAST(MakeDiffFunc0(ident));

			return newast;
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

			auto newast = std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
			newast->AddSubAST(MakeDiffFunc1(ident, funcarg));

			return newast;
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

			auto newast = std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
			newast->AddSubAST(MakeDiffFunc2(ident, funcarg1, funcarg2));

			return newast;
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

			auto diffsym = this->CreateRealConst(0);
			diffsym->SetTableIndex(sym->GetTableIndex());
			diffsym->SetLineRange(sym->GetLineRange());
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

			auto diffsym = this->CreateIntConst(0);
			diffsym->SetTableIndex(sym->GetTableIndex());
			diffsym->SetLineRange(sym->GetLineRange());
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
			const std::string& diffvarname = this->GetDiffVar();

			auto diffident = this->CreateIntConst(varname == diffvarname ? 1 : 0);
			diffident->SetTableIndex(ident->GetTableIndex());
			diffident->SetLineRange(ident->GetLineRange());
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

			auto newast = std::make_shared<ASTUnary>(expr->GetId(), 0,
				arg, op_minus->GetId());
			auto diffast = std::make_shared<ASTUnary>(expr->GetId(), 0,
				diffarg, op_minus->GetId());
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

			auto newast = std::make_shared<ASTUnary>(expr->GetId(), 0,
				arg, op_plus->GetId());
			auto diffast = std::make_shared<ASTUnary>(expr->GetId(), 0,
				diffarg, op_plus->GetId());
			newast->AddSubAST(diffast);

			return newast;
		}));
	}
}



/**
 * create the differential of f(x) ^ g(y)
 */
t_astbaseptr DiffGrammar::MakePowFunc(const t_astbaseptr& arg1, const t_astbaseptr& arg2,
	bool only_diff_ast) const
{
	// df(x)/dx  and  dg(y)/dy
	t_astbaseptr diffarg1 = std::dynamic_pointer_cast<ASTBase>(arg1->GetSubAST(0));
	t_astbaseptr diffarg2 = std::dynamic_pointer_cast<ASTBase>(arg2->GetSubAST(0));

	auto one = this->CreateIntConst(1);
	one->SetLineRange(arg2->GetLineRange());

	// df(x)/dx * g(y)
	auto diffast1_1 = std::make_shared<ASTBinary>(expr->GetId(), 0,
		diffarg1, arg2, op_mult->GetId());
	// g(y) - 1
	auto diffast1_2a = std::make_shared<ASTBinary>(expr->GetId(), 0,
		arg2, one, op_minus->GetId());
	// f(x)^(g(y) - 1)
	auto diffast1_2 = std::make_shared<ASTBinary>(expr->GetId(), 0,
		arg1, diffast1_2a, op_pow->GetId());
	// f(x)^(g(y) - 1) * df(x)/dx * g(y)
	auto diffast1 = std::make_shared<ASTBinary>(expr->GetId(), 0,
		diffast1_1, diffast1_2, op_mult->GetId());

	// f(x)^g(y)
	auto diffast2_1a = std::make_shared<ASTBinary>(expr->GetId(), 0,
		arg1, arg2, op_pow->GetId());
	// dg(y)/dy * f(x)^g(y)
	auto diffast2_1 = std::make_shared<ASTBinary>(expr->GetId(), 0,
		diffarg2, diffast2_1a, op_mult->GetId());

	// log(f(x))
	auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
	funcargs->AddChild(arg1, false);
	auto diffast2_2 = std::make_shared<ASTFuncCall>(expr->GetId(), 0,
		"log", funcargs);

	// dg(y)/dy * f(x)^g(y) * log(f(x))
	auto diffast2 = std::make_shared<ASTBinary>(expr->GetId(), 0,
		diffast2_1, diffast2_2, op_mult->GetId());

	// f(x)^(g(y) - 1) * df(x)/dx * g(y)  +  dg(y)/dy * f(x)^g(y) * log(f(x))
	auto diffast = std::make_shared<ASTBinary>(expr->GetId(), 0,
		diffast1, diffast2, op_plus->GetId());

	if(only_diff_ast)
	{
		// directly return differential ast
		return diffast;
	}
	else
	{
		// attach differential ast as a sub-ast
		auto newast = std::make_shared<ASTBinary>(
			expr->GetId(), 0, arg1, arg2, op_pow->GetId());
		newast->AddSubAST(diffast);
		return newast;
	}

	return nullptr;
}
