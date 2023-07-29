/**
 * grammar commons
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 28-jul-2023
 * @license see 'LICENSE' file
 */

#include "script/grammar_common.h"
#include "script/lexer.h"


/**
 * get index into parse tables
 */
lalr1::t_index GrammarCommon::GetTerminalIndex(lalr1::t_symbol_id id) const
{
	lalr1::t_index tableidx = 0;

	if(m_mapTermIdx)
	{
		auto iter = m_mapTermIdx->find(id);
		if(iter != m_mapTermIdx->end())
			tableidx = iter->second;
	}

	//std::cout << "Id " << id << " -> Idx " << tableidx << std::endl;
	return tableidx;
}


t_astbaseptr GrammarCommon::CreateIntConst(t_int val) const
{
	auto node = std::make_shared<ASTToken<t_int>>(
		GetExprId(), GetTerminalIndex(GetIntId()), val, 0);
	node->SetDataType(VMType::INT);
	node->SetTerminalOverride(false);

	return node;
}


t_astbaseptr GrammarCommon::CreateRealConst(t_real val) const
{
	auto node = std::make_shared<ASTToken<t_real>>(
		GetExprId(), GetTerminalIndex(GetRealId()), val, 0);
	node->SetDataType(VMType::REAL);
	node->SetTerminalOverride(false);

	return node;
}


/**
 * is it a token node with lexical value of zero?
 */
bool GrammarCommon::is_zero_token(const t_astbaseptr& node)
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
bool GrammarCommon::is_one_token(const t_astbaseptr& node)
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
