#
# differentiating expression lexer
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 1-oct-2022
# @license see 'LICENSE' file
#
# References:
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

import re
import numpy as np
from ids import *



#
# pre-compiled regexes
#
re_int   = re.compile("[0-9]+")
re_real  = re.compile("[0-9]+(\\.[0-9]*)?")
re_ident = re.compile("[A-Za-z]+[A-Za-z0-9]*")
re_real_range = re.compile("\\{[+-]?[0-9]+(\\.[0-9]*)?,[ \\t]*[+-]?[0-9]+(\\.[0-9]*)?,[ \\t]*[0-9]+\\}")


def str_to_real_range(str):
	toks = str.strip().split(",")

	from_val = float(toks[0][1:])
	to_val = float(toks[1])
	count_val = int(toks[2][0:-1])

	return np.linspace(from_val, to_val, count_val)


# [ regex parser, token id, converter function from lvalue string to lvalue ]
token_regexes = [
	[ re_int, tok_int_id, lambda str : int(str) ],
	[ re_real, tok_real_id, lambda str : float(str) ],
	[ re_ident, tok_ident_id, lambda str : str ],
	[ re_real_range, tok_real_range_id, str_to_real_range ],
]



#
# get the possibly matching tokens in a string
#
def get_matches(str):
	matches = []

	# get all possible matches
	for to_match in token_regexes:
		the_match = to_match[0].match(str)
		if the_match != None:
			lval = the_match.group()

			matches.append([
				to_match[1],       # token id
				lval,              # token lvalue string
				to_match[2](lval)  # token lvalue
			])

	# sort by match length
	matches = sorted(matches, key=lambda elem : len(elem[1]), reverse=True)
	return matches



#
# get the next token in a string
#
def get_token(str):
	str = str.lstrip()
	if len(str) == 0:
		return [ None, str ]

	matches = get_matches(str)
	if len(matches) > 0:
		return [ [matches[0][0], matches[0][2]],
			str[len(matches[0][1]):] ]

	if str[0]=="+" or str[0]=="-" or \
		str[0]=="*" or str[0]=="/" or \
		str[0]=="%" or str[0]=="^" or \
		str[0]=="(" or str[0]==")" or \
		str[0]=="," or str[0]=="=":
		return [ str[0], str[1:] ]

	return [ None, str ]



#
# get all tokens in a string
#
def get_tokens(str):
	tokens = []
	while True:
		[ token, str ] = get_token(str)
		if token == None:
			break
		tokens.append(token)

	#print(tokens)
	return tokens
