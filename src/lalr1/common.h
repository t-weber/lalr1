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

#include <vector>
#include <unordered_map>


#define ERROR_VAL  0xffffffff  /* 'error' table entry */
#define ACCEPT_VAL 0xfffffffe  /* 'accept' table entry */

#define EPS_IDENT  0xffffff00  /* epsilon token id*/
#define END_IDENT  0xffffff01  /* end token id*/


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
