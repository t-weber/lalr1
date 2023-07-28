/**
 * ast optimiser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date july-2023
 * @license see 'LICENSE' file
 */

#include "ast_opt.h"


ASTOpt::ASTOpt()
{
}


/**
 * is it a token node with lexical value of zero?
 */
bool ASTOpt::is_zero_token(const t_astbaseptr& node)
{
	if(node->GetType() != ASTType::TOKEN)
		return false;

	auto int_node = std::dynamic_pointer_cast<ASTToken<t_int>>(node);
	auto real_node = std::dynamic_pointer_cast<ASTToken<t_real>>(node);

	if(int_node)
		return int_node->GetLexerValue() == t_int(0);
	else if(real_node)
		return real_node->GetLexerValue() == t_real(0);

	return false;
}


/**
 * is it a token node with lexical value of one?
 */
bool ASTOpt::is_one_token(const t_astbaseptr& node)
{
	if(node->GetType() != ASTType::TOKEN)
		return false;

	auto int_node = std::dynamic_pointer_cast<ASTToken<t_int>>(node);
	auto real_node = std::dynamic_pointer_cast<ASTToken<t_real>>(node);

	if(int_node)
		return int_node->GetLexerValue() == t_int(1);
	else if(real_node)
		return real_node->GetLexerValue() == t_real(1);

	return false;
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

	if(ast->GetOpId() == '+')
	{
		// 0 + x = x
		if(is_zero_token(ast->GetChild(0)))
			return ast->GetChild(1);

		// x + 0 = x
		if(is_zero_token(ast->GetChild(1)))
			return ast->GetChild(0);
	}
	else if(ast->GetOpId() == '-')
	{
		// x - 0 = x
		if(is_zero_token(ast->GetChild(1)))
			return ast->GetChild(0);
	}
	else if(ast->GetOpId() == '*')
	{
		// 0 * x = 0
		if(is_zero_token(ast->GetChild(0)))
			return ast->GetChild(0);

		// x * 0 = 0
		if(is_zero_token(ast->GetChild(1)))
			return ast->GetChild(1);

		// 1 * x = x
		if(is_one_token(ast->GetChild(0)))
			return ast->GetChild(1);

		// x * 1 = x
		if(is_one_token(ast->GetChild(1)))
			return ast->GetChild(0);
	}
	else if(ast->GetOpId() == '/')
	{
		// 0 / x = 0
		if(is_zero_token(ast->GetChild(0)))
			return ast->GetChild(0);

		// x / 1 = x
		if(is_one_token(ast->GetChild(1)))
			return ast->GetChild(0);
	}
	else if(ast->GetOpId() == '^')
	{
		// x^1 = x
		if(is_one_token(ast->GetChild(1)))
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
