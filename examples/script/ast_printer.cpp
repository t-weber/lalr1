/**
 * ast printer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14-jun-2022
 * @license see 'LICENSE' file
 */

#include "ast_printer.h"
#include "script_vm/types.h"


/**
 * gets a printable type name of an ast node
 */
std::string ASTPrinter::get_ast_typename(const ::ASTBase* ast)
{
	switch(ast->GetType())
	{
		case ASTType::TOKEN: return "token";
		case ASTType::UNARY: return "unary";
		case ASTType::BINARY: return "binary";
		case ASTType::LIST: return "list";
		case ASTType::CONDITION: return "condition";
		case ASTType::LOOP: return "loop";
		case ASTType::FUNC: return "function";
		case ASTType::FUNCCALL: return "function_call";
		case ASTType::JUMP:
		{
			std::string ty = "jump";
			const auto* jump = static_cast<const ASTJump*>(ast);
			std::string subty = get_jump_typename(jump);
			return ty + "/" + subty;
		}
		case ASTType::DECLARE: return "declaration";
	}

	return "<unknown>";
}


/**
 * gets a printable subtype name of a jump node
 */
std::string ASTPrinter::get_jump_typename(const ASTJump* ast)
{
	switch(ast->GetJumpType())
	{
		case ASTJump::JumpType::RETURN: return "return";
		case ASTJump::JumpType::BREAK: return "break";
		case ASTJump::JumpType::CONTINUE: return "continue";
		case ASTJump::JumpType::UNKNOWN: return "<unknown>";
	}

	return "<unknonw>";
}


ASTPrinter::ASTPrinter(std::ostream& ostr) : m_ostr{ostr}
{
}


void ASTPrinter::print_base(
	const ::ASTBase* ast, std::size_t level, const char* extrainfo)
{
	for(std::size_t i=0; i<level; ++i)
		m_ostr << "  ";
	m_ostr << get_ast_typename(ast) << ", id = " << ast->GetId();
	if(const auto& line_range = ast->GetLineRange(); line_range)
	{
		auto start_line = std::get<0>(*line_range);
		auto end_line = std::get<1>(*line_range);

		if(start_line == end_line)
			m_ostr << ", line = " << start_line;
		else
			m_ostr << ", lines = [" << start_line
				<< ", " << end_line << "]";
	}
	m_ostr << ", data type = " << get_vm_type_name(ast->GetDataType());
	//if(ast->GetId() < 256)
	//	m_ostr << " (" << (char)ast->GetId() << ")";
	if(extrainfo)
		m_ostr << extrainfo;
	m_ostr << "\n";

	for(std::size_t i=0; i<ast->NumChildren(); ++i)
	{
		if(!ast->GetChild(i))
			continue;
		ast->GetChild(i)->accept(this, level+1);
	}
}


template<class t_val>
static std::string print_token(const ASTToken<t_val>* ast)
{
	std::ostringstream ostr;

	if(ast->HasLexerValue())
		ostr << ", value = " << ast->GetLexerValue();

	return ostr.str();
}


void ASTPrinter::visit(
	[[maybe_unused]] const ASTToken<t_lval>* ast,
	[[maybe_unused]] std::size_t level)
{
	// TODO: implement for variant type t_lval
	std::cerr << "Error: " << __func__ << " not implemented." << std::endl;
	//std::string tok = print_token(ast);
	//print_base(ast, level, tok.c_str());
}


void ASTPrinter::visit(const ASTToken<t_real>* ast, std::size_t level)
{
	std::string tok = print_token(ast);
	print_base(ast, level, tok.c_str());
}


void ASTPrinter::visit(const ASTToken<t_int>* ast, std::size_t level)
{
	std::string tok = print_token(ast);
	print_base(ast, level, tok.c_str());
}


void ASTPrinter::visit(const ASTToken<std::string>* ast, std::size_t level)
{
	std::string tok = print_token(ast);
	print_base(ast, level, tok.c_str());
}


void ASTPrinter::visit(const ASTToken<void*>* ast, std::size_t level)
{
	std::string tok = print_token(ast);
	print_base(ast, level, tok.c_str());
}


void ASTPrinter::visit(const ASTUnary* ast, std::size_t level)
{
	std::ostringstream _ostr;
	_ostr << ", op = " << ast->GetOpId();
	if(ast->GetOpId() < 256)
		_ostr << " (" << (char)ast->GetOpId() << ")";
	print_base(ast, level, _ostr.str().c_str());
}


void ASTPrinter::visit(const ASTBinary* ast, std::size_t level)
{
	std::ostringstream _ostr;
	_ostr << ", op = " << ast->GetOpId();
	if(ast->GetOpId() < 256)
		_ostr << " (" << (char)ast->GetOpId() << ")";
	print_base(ast, level, _ostr.str().c_str());
}


void ASTPrinter::visit(const ASTList* ast, std::size_t level)
{
	print_base(ast, level);
}


void ASTPrinter::visit(const ASTCondition* ast, std::size_t level)
{
	print_base(ast, level);
}


void ASTPrinter::visit(const ASTLoop* ast, std::size_t level)
{
	print_base(ast, level);
}


void ASTPrinter::visit(const ASTFunc* ast, std::size_t level)
{
	print_base(ast, level);
}


void ASTPrinter::visit(const ASTFuncCall* ast, std::size_t level)
{
	print_base(ast, level);
}


void ASTPrinter::visit(const ASTJump* ast, std::size_t level)
{
	print_base(ast, level);
}


void ASTPrinter::visit(const ASTDeclare* ast, std::size_t level)
{
	print_base(ast, level);
}
