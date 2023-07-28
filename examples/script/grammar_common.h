/**
 * grammar commons
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 28-jul-2023
 * @license see 'LICENSE' file
 */
#ifndef __LALR1_SCRIPT_GRAMMAR_COMMON_H__
#define __LALR1_SCRIPT_GRAMMAR_COMMON_H__


#include "core/symbol.h"
#include "core/common.h"
#include "script/ast.h"


class GrammarCommon
{
public:
	virtual ~GrammarCommon() = default;

	void SetTermIdxMap(const lalr1::t_mapIdIdx* map) { m_mapTermIdx = map; }
	lalr1::t_index GetTerminalIndex(lalr1::t_symbol_id id) const;

	// get the grammar-specific constants
	virtual lalr1::t_symbol_id GetIntId() const = 0;
	virtual lalr1::t_symbol_id GetRealId() const = 0;
	virtual lalr1::t_symbol_id GetExprId() const = 0;

	t_astbaseptr CreateIntConst(t_int val) const;
	t_astbaseptr CreateRealConst(t_real val) const;


protected:
	// terminal index map
	const lalr1::t_mapIdIdx* m_mapTermIdx{nullptr};
};

#endif
