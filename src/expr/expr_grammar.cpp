/**
 * expression grammar example
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-jun-2022
 * @license see 'LICENSE' file
 */

#include "expr_grammar.h"
#include "script/lexer.h"


void ExprGrammar::CreateGrammar(bool add_rules, bool add_semantics)
{
	// non-terminals
	start = std::make_shared<NonTerminal>(START, "start");
	expr = std::make_shared<NonTerminal>(EXPR, "expr");

	// terminals
	//op_assign = std::make_shared<Terminal>('=', "=");
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
	//op_assign->SetPrecedence(10, 'r');
	op_plus->SetPrecedence(70, 'l');
	op_minus->SetPrecedence(70, 'l');
	op_mult->SetPrecedence(80, 'l');
	op_div->SetPrecedence(80, 'l');
	op_mod->SetPrecedence(80, 'l');
	op_pow->SetPrecedence(110, 'r');


	// rule number
	std::size_t semanticindex = 0;

	// rule 0: start -> expr
	if(add_rules)
	{
		start->AddRule({ expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			return args[0];
		});
	}

	// rule 1: expr -> expr + expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_plus, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_plus->GetId());
		});
	}


	// rule 2: expr -> expr - expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_minus, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_minus->GetId());
		});
	}


	// rule 3: expr -> expr * expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_mult, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_mult->GetId());
		});
	}


	// rule 4: expr -> expr / expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_div, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_div->GetId());
		});
	}


	// rule 5: expr -> expr % expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_mod, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(expr->GetId(), 0, arg1, arg2, op_mod->GetId());
		});
	}


	// rule 6: expr -> expr ^ expr
	if(add_rules)
	{
		expr->AddRule({ expr, op_pow, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr arg1 = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr arg2 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			return std::make_shared<ASTBinary>(
				expr->GetId(), 0, arg1, arg2, op_pow->GetId());
		});
	}


	// rule 7: expr -> ( expr )
	if(add_rules)
	{
		expr->AddRule({ bracket_open, expr, bracket_close }, semanticindex++);
	}
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
	if(add_rules)
	{
		expr->AddRule({ ident, bracket_open, bracket_close }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);

			return std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
		});
	}


	// rule 9: expr -> ident(expr)
	if(add_rules)
	{
		expr->AddRule({ ident, bracket_open, expr, bracket_close }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
			t_astbaseptr funcarg = std::dynamic_pointer_cast<ASTBase>(args[2]);
			funcargs->AddChild(funcarg, false);

			return std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
		});
	}


	// rule 10: expr -> ident(expr, expr)
	if(add_rules)
	{
		expr->AddRule({ ident, bracket_open, expr, comma, expr, bracket_close },
			semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			auto funcname = std::dynamic_pointer_cast<ASTToken<std::string>>(args[0]);
			funcname->SetIdent(true);
			const std::string& ident = funcname->GetLexerValue();

			t_astbaseptr funcarg1 = std::dynamic_pointer_cast<ASTBase>(args[2]);
			t_astbaseptr funcarg2 = std::dynamic_pointer_cast<ASTBase>(args[4]);
			auto funcargs = std::make_shared<ASTList>(expr->GetId(), 0);
			funcargs->AddChild(funcarg2, false);
			funcargs->AddChild(funcarg1, false);

			return std::make_shared<ASTFuncCall>(expr->GetId(), 0, ident, funcargs);
		});
	}


	// rule 11: expr -> real symbol
	if(add_rules)
	{
		expr->AddRule({ sym_real }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr sym = std::dynamic_pointer_cast<ASTBase>(args[0]);
			sym->SetDataType(VMType::REAL);
			sym->SetId(expr->GetId());
			sym->SetTerminalOverride(false);  // expression, no terminal any more
			return sym;
		});
	}


	// rule 12: expr -> int symbol
	if(add_rules)
	{
		expr->AddRule({ sym_int }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr sym = std::dynamic_pointer_cast<ASTBase>(args[0]);
			sym->SetDataType(VMType::INT);
			sym->SetId(expr->GetId());
			sym->SetTerminalOverride(false);  // expression, no terminal any more
			return sym;
		});
	}


	// rule 13: expr -> ident
	if(add_rules)
	{
		expr->AddRule({ ident }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr ident = std::dynamic_pointer_cast<ASTBase>(args[0]);
			ident->SetDataType(VMType::INT);
			ident->SetId(expr->GetId());
			ident->SetTerminalOverride(false);  // expression, no terminal any more
			return ident;
		});
	}


	// rule 14, unary-: expr -> -expr
	if(add_rules)
	{
		expr->AddRule({ op_minus, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr arg = std::dynamic_pointer_cast<ASTBase>(args[1]);
			return std::make_shared<ASTUnary>(expr->GetId(), 0, arg, op_minus->GetId());
		});
	}


	// rule 15, unary+: expr -> +expr
	if(add_rules)
	{
		expr->AddRule({ op_plus, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr arg = std::dynamic_pointer_cast<ASTBase>(args[1]);
			return std::make_shared<ASTUnary>(expr->GetId(), 0, arg, op_plus->GetId());
		});
	}


	// rule 16, assignment: expr -> ident = expr
	/*if(add_rules)
	{
		expr->AddRule({ ident, op_assign, expr }, semanticindex++);
	}
	if(add_semantics)
	{
		rules.emplace_back(
		[this](const std::vector<t_lalrastbaseptr>& args) -> t_lalrastbaseptr
		{
			t_astbaseptr rhsident = std::dynamic_pointer_cast<ASTBase>(args[0]);
			t_astbaseptr rhsexpr = std::dynamic_pointer_cast<ASTBase>(args[2]);

			if(rhsident->GetType() != ASTType::TOKEN)
			{
				throw std::runtime_error(
					"Expected a symbol name on lhs of assignment.");
			}

			auto symname = std::dynamic_pointer_cast<ASTToken<std::string>>(rhsident);
			symname->SetIdent(true);
			symname->SetLValue(true);
			symname->SetDataType(rhsexpr->GetDataType());

			return std::make_shared<ASTBinary>(expr->GetId(), 0, rhsexpr, symname, op_assign->GetId());
		});
	}*/
}
