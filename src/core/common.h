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
#include <stack>
#include <unordered_map>
#include <string>
#include <locale>


namespace lalr1 {

const constinit t_index ERROR_VAL = std::numeric_limits<t_index>::max();     // 'error' table entry
const constinit t_index ACCEPT_VAL = std::numeric_limits<t_index>::max()-1;  // 'accept' table entry

const constinit t_symbol_id EPS_IDENT = std::numeric_limits<t_symbol_id>::max()-2;  // epsilon token id
const constinit t_symbol_id END_IDENT = std::numeric_limits<t_symbol_id>::max()-3;  // end token id


// (input) token types
using t_toknode = t_astbaseptr;
using t_toknodes = std::vector<t_astbaseptr>;

// LALR(1) table types
using t_table = Table<t_index, std::vector>;
using t_mapIdIdx = std::unordered_map<t_symbol_id, t_index>;
using t_mapIdStrId = std::unordered_map<t_symbol_id, std::string>;
using t_vecIdx = std::vector<t_index>;

using t_mapSemanticIdIdx = std::unordered_map<t_semantic_id, t_index>;
using t_mapSemanticIdxId = std::unordered_map<t_index, t_semantic_id>;

enum class IndexTableKind
{
	TERMINAL,
	NONTERMINAL,
	SEMANTIC,
};


struct ActiveRule
{
	t_index seen_tokens = 0;     // number of tokens already seen in partial match
	t_index handle = 0;          // the same for corresponding partial rules
	t_astbaseptr retval{};       // partial semantic rule return value
};

using t_active_rule_stack = std::stack<ActiveRule>;
using t_active_rules = std::unordered_map<t_semantic_id, t_active_rule_stack>;


template<class T> bool isprintable(T ch)
{
	if(sizeof(ch) > 1 && ch > 255)
		return false;

	using t_char = char;
	return std::isprint<t_char>(static_cast<t_char>(ch), std::locale("C"));
}


template<class t_str = std::string, class t_ch = typename t_str::value_type>
t_str get_escaped_char(t_ch ch)
{
	t_str str;

	if(ch == '\'' || ch == '\\')
		str.push_back('\\');
	str.push_back(ch);

	return str;
}


} // namespace lalr1

#endif
