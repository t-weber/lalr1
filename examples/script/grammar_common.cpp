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
