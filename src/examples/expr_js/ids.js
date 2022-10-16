/**
 * expression parser ids
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 16-oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

//
// semantic ids
//
const SemanticIds =
{
	start    : 100,
	brackets : 101,
	add      : 200,
	sub      : 201,
	mul      : 202,
	div      : 203,
	mod      : 204,
	pow      : 205,
	uadd     : 210,
	usub     : 211,
	call0    : 300,
	call1    : 301,
	call2    : 302,
	real     : 400,
	int      : 401,
	ident    : 410,
	assign   : 500
};


//
// token ids
//
const TokenIds =
{
	real     : 1000,
	int      : 1001,
	str      : 1002,
	ident    : 1003
};


//
// nonterminal ids
//
const NonTerminalIds =
{
	start   : 10,
	expr    : 20
};


module.exports =
{
	"SemanticIds" : SemanticIds,
	"TokenIds" : TokenIds,
	"NonTerminalIds" : NonTerminalIds,
};
