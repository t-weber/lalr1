/**
 * ast optimiser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date july-2023
 * @license see 'LICENSE' file
 */

#include "ast_opt.h"
#include "script_vm/helpers.h"  // for pow()


ASTOpt::ASTOpt(const GrammarCommon* grammar)
	: m_grammar{grammar}
{ }


ASTOpt::ASTOpt(const ASTOpt& other)
	: m_grammar(other.m_grammar)
{ }


ASTOpt& ASTOpt::operator=(const ASTOpt& other)
{
	this->m_grammar = other.m_grammar;
	return *this;
}


/**
 * call optimiser for child nodes
 */
void ASTOpt::recurse(::ASTBase* ast, std::size_t level)
{
	for(std::size_t i=0; i<ast->NumChildren(); ++i)
	{
		if(!ast->GetChild(i))
			continue;

		// set new child node if it has changed
		t_astbaseptr newnode = ast->GetChild(i)->accept(this, level+1);
		if(newnode)
			ast->SetChild(i, newnode);
	}
}


t_astbaseptr ASTOpt::visit(
	[[maybe_unused]] ASTToken<t_lval>* ast,
	[[maybe_unused]] std::size_t level)
{
	return nullptr;
}


t_astbaseptr ASTOpt::visit(
	[[maybe_unused]] ASTToken<t_real>* ast,
	[[maybe_unused]] std::size_t level)
{
	return nullptr;
}


t_astbaseptr ASTOpt::visit(
	[[maybe_unused]] ASTToken<t_int>* ast,
	[[maybe_unused]] std::size_t level)
{
	return nullptr;
}


t_astbaseptr ASTOpt::visit(
	[[maybe_unused]] ASTToken<std::string>* ast,
	[[maybe_unused]] std::size_t level)
{
	return nullptr;
}


t_astbaseptr ASTOpt::visit(
	[[maybe_unused]] ASTToken<void*>* ast,
	[[maybe_unused]] std::size_t level)
{
	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTUnary* ast, std::size_t level)
{
	recurse(ast, level);
	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTBinary* ast, std::size_t level)
{
	recurse(ast, level);

	// get lexical values
	auto [is_int_0, int_val_0] = GrammarCommon::is_int_const(ast->GetChild(0));
	auto [is_int_1, int_val_1] = GrammarCommon::is_int_const(ast->GetChild(1));
	auto [is_real_0, real_val_0] = GrammarCommon::is_real_const(ast->GetChild(0));
	auto [is_real_1, real_val_1] = GrammarCommon::is_real_const(ast->GetChild(1));

	if(ast->GetOpId() == '+')
	{
		// 0 + x = x
		if(GrammarCommon::is_zero_token(ast->GetChild(0)))
			return ast->GetChild(1);

		// x + 0 = x
		if(GrammarCommon::is_zero_token(ast->GetChild(1)))
			return ast->GetChild(0);

		// in case of constants: add them
		if(is_int_0 && is_int_1)
			return m_grammar->CreateIntConst(int_val_0 + int_val_1);
		else if(is_real_0 && is_real_1)
			return m_grammar->CreateRealConst(real_val_0 + real_val_1);
		else if(is_real_0 && is_int_1)
			return m_grammar->CreateRealConst(real_val_0 + t_real(int_val_1));
		else if(is_int_0 && is_real_1)
			return m_grammar->CreateRealConst(t_real(int_val_0) + real_val_1);
	}
	else if(ast->GetOpId() == '-')
	{
		// x - 0 = x
		if(GrammarCommon::is_zero_token(ast->GetChild(1)))
			return ast->GetChild(0);

		// 0 - x = -x
		if(GrammarCommon::is_zero_token(ast->GetChild(0)))
			return std::make_shared<ASTUnary>(m_grammar->GetExprId(), 0, ast->GetChild(1), '-');

		// in case of constants: subtract them
		if(is_int_0 && is_int_1)
			return m_grammar->CreateIntConst(int_val_0 - int_val_1);
		else if(is_real_0 && is_real_1)
			return m_grammar->CreateRealConst(real_val_0 - real_val_1);
		else if(is_real_0 && is_int_1)
			return m_grammar->CreateRealConst(real_val_0 - t_real(int_val_1));
		else if(is_int_0 && is_real_1)
			return m_grammar->CreateRealConst(t_real(int_val_0) - real_val_1);
	}
	else if(ast->GetOpId() == '*')
	{
		// 0 * x = 0
		if(GrammarCommon::is_zero_token(ast->GetChild(0)))
			return ast->GetChild(0);

		// x * 0 = 0
		if(GrammarCommon::is_zero_token(ast->GetChild(1)))
			return ast->GetChild(1);

		// 1 * x = x
		if(GrammarCommon::is_one_token(ast->GetChild(0)))
			return ast->GetChild(1);

		// x * 1 = x
		if(GrammarCommon::is_one_token(ast->GetChild(1)))
			return ast->GetChild(0);

		// in case of constants: multiply them
		if(is_int_0 && is_int_1)
			return m_grammar->CreateIntConst(int_val_0 * int_val_1);
		else if(is_real_0 && is_real_1)
			return m_grammar->CreateRealConst(real_val_0 * real_val_1);
		else if(is_real_0 && is_int_1)
			return m_grammar->CreateRealConst(real_val_0 * t_real(int_val_1));
		else if(is_int_0 && is_real_1)
			return m_grammar->CreateRealConst(t_real(int_val_0) * real_val_1);
	}
	else if(ast->GetOpId() == '/')
	{
		// 0 / x = 0
		if(GrammarCommon::is_zero_token(ast->GetChild(0)))
			return ast->GetChild(0);

		// x / 1 = x
		if(GrammarCommon::is_one_token(ast->GetChild(1)))
			return ast->GetChild(0);

		// in case of constants: divide them
		if(is_int_0 && is_int_1)
			return m_grammar->CreateIntConst(int_val_0 / int_val_1);
		else if(is_real_0 && is_real_1)
			return m_grammar->CreateRealConst(real_val_0 / real_val_1);
		else if(is_real_0 && is_int_1)
			return m_grammar->CreateRealConst(real_val_0 / t_real(int_val_1));
		else if(is_int_0 && is_real_1)
			return m_grammar->CreateRealConst(t_real(int_val_0) / real_val_1);
	}
	else if(ast->GetOpId() == '%')
	{
		// 0 % x = 0
		if(GrammarCommon::is_zero_token(ast->GetChild(0)))
			return ast->GetChild(0);

		// in case of constants: calculate the modulo
		if(is_int_0 && is_int_1)
			return m_grammar->CreateIntConst(int_val_0 % int_val_1);
		else if(is_real_0 && is_real_1)
			return m_grammar->CreateRealConst(std::fmod(real_val_0, real_val_1));
		else if(is_real_0 && is_int_1)
			return m_grammar->CreateRealConst(std::fmod(real_val_0, t_real(int_val_1)));
		else if(is_int_0 && is_real_1)
			return m_grammar->CreateRealConst(std::fmod(t_real(int_val_0), real_val_1));
	}
	else if(ast->GetOpId() == '^')
	{
		// x^1 = x
		if(GrammarCommon::is_one_token(ast->GetChild(1)))
			return ast->GetChild(0);

		// x^0 = 1;
		if(GrammarCommon::is_zero_token(ast->GetChild(1)))
			return m_grammar->CreateIntConst(1);

		// 1^x = 1
		if(GrammarCommon::is_one_token(ast->GetChild(0)))
			return ast->GetChild(0);

		// in case of constants: take the power
		if(is_int_0 && is_int_1)
			return m_grammar->CreateIntConst(pow<t_int>(int_val_0, int_val_1));
		else if(is_real_0 && is_real_1)
			return m_grammar->CreateRealConst(pow<t_real>(real_val_0, real_val_1));
		else if(is_real_0 && is_int_1)
			return m_grammar->CreateRealConst(pow<t_real>(real_val_0, t_real(int_val_1)));
		else if(is_int_0 && is_real_1)
			return m_grammar->CreateRealConst(pow<t_real>(t_real(int_val_0), real_val_1));
	}

	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTList* ast, std::size_t level)
{
	recurse(ast, level);
	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTCondition* ast, std::size_t level)
{
	recurse(ast, level);
	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTLoop* ast, std::size_t level)
{
	recurse(ast, level);
	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTFunc* ast, std::size_t level)
{
	recurse(ast, level);
	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTFuncCall* ast, std::size_t level)
{
	recurse(ast, level);
	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTJump* ast, std::size_t level)
{
	recurse(ast, level);
	return nullptr;
}


t_astbaseptr ASTOpt::visit(ASTDeclare* ast, std::size_t level)
{
	recurse(ast, level);
	return nullptr;
}
