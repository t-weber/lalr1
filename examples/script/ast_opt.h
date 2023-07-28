/**
 * ast optimiser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date july-2023
 * @license see 'LICENSE' file
 */

#ifndef __LR1_AST_OPT_H__
#define __LR1_AST_OPT_H__


#include "lval.h"
#include "ast.h"


class ASTOpt : public ASTModifyingVisitor
{
public:
	ASTOpt();

	virtual t_astbaseptr visit(ASTToken<t_lval>* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTToken<t_real>* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTToken<t_int>* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTToken<std::string>* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTToken<void*>* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTUnary* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTBinary* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTList* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTCondition* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTLoop* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTFunc* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTFuncCall* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTJump* ast, std::size_t level) override;
	virtual t_astbaseptr visit(ASTDeclare* ast, std::size_t level) override;


protected:
	void recurse(::ASTBase* ast, std::size_t level);

	static bool is_zero_token(const t_astbaseptr& node);
	static bool is_one_token(const t_astbaseptr& node);
};


#endif
