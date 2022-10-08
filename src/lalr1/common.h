/**
 * common defines and types
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_COMMON_H__
#define __LALR1_COMMON_H__

#include "table.h"
#include "ast.h"

#include <limits>
#include <vector>
#include <unordered_map>


const constinit t_index ERROR_VAL = std::numeric_limits<t_index>::max();     // 'error' table entry
const constinit t_index ACCEPT_VAL = std::numeric_limits<t_index>::max()-1;  // 'accept' table entry

const constinit t_symbol_id EPS_IDENT = std::numeric_limits<t_symbol_id>::max()-2;  // epsilon token id
const constinit t_symbol_id END_IDENT = std::numeric_limits<t_symbol_id>::max()-3;  // end token id


// (input) token types
using t_toknode = t_lalrastbaseptr;
using t_toknodes = std::vector<t_lalrastbaseptr>;

// LALR(1) table types
using t_table = Table<std::size_t, std::vector>;
using t_mapIdIdx = std::unordered_map<t_symbol_id, std::size_t>;
using t_vecIdx = std::vector<std::size_t>;

using t_mapSemanticIdIdx = std::unordered_map<t_semantic_id, std::size_t>;
using t_mapSemanticIdxId = std::unordered_map<std::size_t, t_semantic_id>;

enum class IndexTableKind
{
	TERMINAL,
	NONTERMINAL,
	SEMANTIC,
};


#endif
