/**
 * ast optimiser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date july-2023
 * @license see 'LICENSE' file
 */

#include "ast_opt.h"


ASTOpt::ASTOpt(const GrammarCommon* grammar)
	: m_grammar{grammar}
{ }


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

	if(ast->GetOpId() == '+')
	{
		// 0 + x = x
		if(GrammarCommon::is_zero_token(ast->GetChild(0)))
			return ast->GetChild(1);

		// x + 0 = x
		if(GrammarCommon::is_zero_token(ast->GetChild(1)))
			return ast->GetChild(0);
	}
	else if(ast->GetOpId() == '-')
	{
		// x - 0 = x
		if(GrammarCommon::is_zero_token(ast->GetChild(1)))
			return ast->GetChild(0);
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
	}
	else if(ast->GetOpId() == '/')
	{
		// 0 / x = 0
		if(GrammarCommon::is_zero_token(ast->GetChild(0)))
			return ast->GetChild(0);

		// x / 1 = x
		if(GrammarCommon::is_one_token(ast->GetChild(1)))
			return ast->GetChild(0);
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
