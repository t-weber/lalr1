/**
 * expression lexer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 16-oct-2022
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 */

const ids = require("./ids.js")


// pre-compiled regexes
const re_int   = /[0-9]+/;
const re_real  = /[0-9]+(\.[0-9]*)?/;
const re_ident = /[A-Za-z]+[A-Za-z0-9]*/;

// [ regex parser, token id, converter function from lvalue string to lvalue ]
const token_regexes = [
        [ re_int, ids.TokenIds.int, (str) => { return parseInt(str); } ],
        [ re_real, ids.TokenIds.real, (str) => { return parseFloat(str); } ],
        [ re_ident, ids.TokenIds.ident, (str) => { return str; } ],
];


/**
 * get the possibly matching tokens in a string
 */
function get_matches(str)
{
	let matches = [];

        // get all possible matches
	for(let to_match of token_regexes)
	{
		const the_match = str.match(to_match[0]);

		if(the_match != null && the_match.index == 0)
		{
			const lval = the_match[0];

			matches.push([
				to_match[1],       // token id
				lval,              // token lvalue string
				to_match[2](lval)  // token lvalue
			]);
		}
	}

	// sort by match length
	matches.sort((elem1, elem2) => {return elem2[1].length - elem1[1].length});
	return matches;
}


/**
 * get the next token in a string
 */
function get_token(str)
{
	str = str.trimLeft();
	if(str.length == 0)
		return [ null, str ];

	let matches = get_matches(str);
	if(matches.length > 0)
	{
		return [ [matches[0][0], matches[0][2]],
			str.substring(matches[0][1].length) ];
	}

	if(str[0]=="+" || str[0]=="-" ||
		str[0]=="*" || str[0]=="/" ||
		str[0]=="%" || str[0]=="^" ||
		str[0]=="(" || str[0]==")" ||
		str[0]==",")
	{
		return [ str[0], str.substr(1) ];
	}

	return [ null, str ];
}


/**
 * get all tokens in a string
 */
function get_tokens(str)
{
	let tokens = [];
	while(true)
	{
		const [ token, str_rest ] = get_token(str);
		if(token == null)
			break;
		tokens.push(token);
		str = str_rest;
	}

	return tokens;
}


module.exports =
{
	"get_tokens" : get_tokens,
};
