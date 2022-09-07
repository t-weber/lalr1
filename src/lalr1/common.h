/**
 * common types
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_TYPES_H__
#define __LALR1_TYPES_H__

#include "table.h"
#include "ast.h"

#include <vector>
#include <unordered_map>
#include <variant>
#include <tuple>


#define ERROR_VAL  0xffffffff  /* 'error' table entry */
#define ACCEPT_VAL 0xfffffffe  /* 'accept' table entry */

#define EPS_IDENT  0xffffff00  /* epsilon token id*/
#define END_IDENT  0xffffff01  /* end token id*/


using t_toknode = t_lalrastbaseptr;

using t_table = Table<std::size_t, std::vector>;
using t_mapIdIdx = std::unordered_map<std::size_t, std::size_t>;
using t_vecIdx = std::vector<std::size_t>;


/**
 * partial match of a rule
 */
struct PartialMatch
{
	std::size_t matched_len{0};
	std::vector<std::size_t> lookahead_indices{};
};


// partial match per rule number
using t_partialmatch = std::unordered_map<std::size_t, PartialMatch>;
using t_partialmatches = std::vector<t_partialmatch>;


#endif
