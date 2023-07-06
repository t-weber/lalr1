/**
 * ast printer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14-jun-2022
 * @license see 'LICENSE' file
 */

#ifndef __LR1_AST_PRINTER_H__
#define __LR1_AST_PRINTER_H__

#include <iostream>

#include "lval.h"
#include "ast.h"



class ASTPrinter : public ASTVisitor
{
public:
	ASTPrinter(std::ostream& ostr = std::cout);

	virtual void visit(const ASTToken<t_lval>* ast, std::size_t level) override;
	virtual void visit(const ASTToken<t_real>* ast, std::size_t level) override;
	virtual void visit(const ASTToken<t_int>* ast, std::size_t level) override;
	virtual void visit(const ASTToken<std::string>* ast, std::size_t level) override;
	virtual void visit(const ASTToken<void*>* ast, std::size_t level) override;
	virtual void visit(const ASTUnary* ast, std::size_t level) override;
	virtual void visit(const ASTBinary* ast, std::size_t level) override;
	virtual void visit(const ASTList* ast, std::size_t level) override;
	virtual void visit(const ASTCondition* ast, std::size_t level) override;
	virtual void visit(const ASTLoop* ast, std::size_t level) override;
	virtual void visit(const ASTFunc* ast, std::size_t level) override;
	virtual void visit(const ASTFuncCall* ast, std::size_t level) override;
	virtual void visit(const ASTJump* ast, std::size_t level) override;
	virtual void visit(const ASTDeclare* ast, std::size_t level) override;

	static std::string get_ast_typename(const ::ASTBase* ast);
	static std::string get_jump_typename(const ASTJump* ast);


protected:
	void print_base(const ::ASTBase* ast, std::size_t level,
		const char *extrainfo = nullptr);


private:
	std::ostream& m_ostr{std::cout};
};


#endif
