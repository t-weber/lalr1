/**
 * expression printer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 29-jul-2023
 * @license see 'LICENSE' file
 */

#include "expr/printer.h"
#include "script_vm/types.h"


ExprPrinter::ExprPrinter(std::ostream& ostr) : m_ostr{ostr}
{
}


void ExprPrinter::print_children(const ::ASTBase* ast, std::size_t level)
{
	for(std::size_t i = 0; i < ast->NumChildren(); ++i)
	{
		if(!ast->GetChild(i))
			continue;
		ast->GetChild(i)->accept(this, level+1);
	}
}


void ExprPrinter::visit(
	[[maybe_unused]] const ASTToken<t_lval>* ast,
	[[maybe_unused]] std::size_t level)
{
	// TODO: implement for variant type t_lval
	std::cerr << "Error: " << __func__ << " not implemented." << std::endl;
}


void ExprPrinter::visit(const ASTToken<t_real>* ast, [[maybe_unused]] std::size_t level)
{
	if(ast->HasLexerValue())
		m_ostr << ast->GetLexerValue();
}


void ExprPrinter::visit(const ASTToken<t_int>* ast, [[maybe_unused]] std::size_t level)
{
	if(ast->HasLexerValue())
		m_ostr << ast->GetLexerValue();
}


void ExprPrinter::visit(const ASTToken<std::string>* ast, [[maybe_unused]] std::size_t level)
{
	if(ast->HasLexerValue())
		m_ostr << ast->GetLexerValue();
}


void ExprPrinter::visit(const ASTToken<void*>* ast, [[maybe_unused]] std::size_t level)
{
	if(ast->HasLexerValue())
		m_ostr << ast->GetLexerValue();
}


void ExprPrinter::visit(const ASTUnary* ast, std::size_t level)
{
	if(ast->GetOpId() == '+')
	{
		ast->GetChild(0)->accept(this, level+1);
	}
	else
	{
		m_ostr << "(";
		if(ast->GetOpId() < 256)
			m_ostr << (char)ast->GetOpId();
		else
			m_ostr << "op_" << ast->GetOpId() << " ";

		ast->GetChild(0)->accept(this, level+1);
		m_ostr << ")";
	}
}


void ExprPrinter::visit(const ASTBinary* ast, std::size_t level)
{
	m_ostr << "(";
	ast->GetChild(0)->accept(this, level+1);

	if(ast->GetOpId() < 256)
		m_ostr << (char)ast->GetOpId();
	else
		m_ostr << "op_" << ast->GetOpId() << " ";

	ast->GetChild(1)->accept(this, level+1);
	m_ostr << ")";
}


void ExprPrinter::visit(const ASTList* ast, std::size_t level)
{
	std::size_t N = ast->NumChildren();
	for(std::size_t i = 0; i < N; ++i)
	{
		ast->GetChild(N-i-1)->accept(this, level+1);
		if(i < N-1)
			m_ostr << ", ";
	}
}


void ExprPrinter::visit(const ASTCondition* ast, std::size_t level)
{
	print_children(ast, level);
}


void ExprPrinter::visit(const ASTLoop* ast, std::size_t level)
{
	print_children(ast, level);
}


void ExprPrinter::visit(const ASTFunc* ast, std::size_t level)
{
	print_children(ast, level);
}


void ExprPrinter::visit(const ASTFuncCall* ast, std::size_t level)
{
	m_ostr << ast->GetName() << "(";

	for(std::size_t i = 0; i < ast->NumChildren(); ++i)
		ast->GetChild(i)->accept(this, level+1);

	m_ostr << ")";
}


void ExprPrinter::visit(const ASTJump* ast, std::size_t level)
{
	print_children(ast, level);
}


void ExprPrinter::visit(const ASTDeclare* ast, std::size_t level)
{
	print_children(ast, level);
}
