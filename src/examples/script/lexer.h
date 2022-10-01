/**
 * simple lexer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 7-mar-20
 * @license see 'LICENSE' file
 */

#ifndef __LR1_LEXER_H__
#define __LR1_LEXER_H__

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <optional>

#include "ast.h"
#include "lval.h"
#include "lalr1/common.h"


using t_tok = std::size_t;

// [ token, lvalue, line number ]
using t_lexer_match = std::tuple<t_tok, t_lval, std::size_t>;


enum class Token : t_tok
{
	REAL        = 1000,
	INT         = 1001,
	STR         = 1002,
	IDENT       = 1003,

	EQU         = 2000,
	NEQU        = 2001,
	GEQU        = 2002,
	LEQU        = 2003,

	// logical operators
	AND         = 3000,
	OR          = 3001,

	// binary operators
	BIN_XOR     = 3100,

	IF          = 4000,
	ELSE        = 4001,

	LOOP        = 5000,
	BREAK       = 5001,
	CONTINUE    = 5002,

	FUNC        = 6000,
	RETURN      = 6001,
	EXTERN      = 6002,

	SHIFT_LEFT  = 7000,
	SHIFT_RIGHT = 7001,

	END         = END_IDENT,
};


class Lexer
{
public:
	Lexer(std::istream* = &std::cin);

	// get all tokens and attributes
	std::vector<t_toknode> GetAllTokens();

	void SetEndOnNewline(bool b) { m_end_on_newline = b; }
	void SetIgnoreInt(bool b) { m_ignore_int = b; }
	void SetTermIdxMap(const t_mapIdIdx* map) { m_mapTermIdx = map; }


protected:
	// get next token and attribute
	t_lexer_match GetNextToken(std::size_t* line = nullptr);

	// find all matching tokens for input string
	std::vector<t_lexer_match> GetMatchingTokens(
		const std::string& str, std::size_t line);


private:
	bool m_end_on_newline{true};
	bool m_ignore_int{false};

	std::istream* m_istr{nullptr};
	const t_mapIdIdx* m_mapTermIdx{nullptr};
};


#endif
