/**
 * expression printer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 29-jul-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_EXPR_PRINTER_H__
#define __LALR1_EXPR_PRINTER_H__

#include <iostream>

#include "script/lval.h"
#include "script/ast.h"



class ExprPrinter : public ASTVisitor
{
public:
	ExprPrinter(std::ostream& ostr = std::cout);

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


protected:
	void print_children(const ::ASTBase* ast, std::size_t level);


private:
	std::ostream& m_ostr{std::cout};
};


#endif
