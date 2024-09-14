#
# expression lexer
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 7-sep-2024
# @license see 'LICENSE' file
#
# References:
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

module lexer

using ids


#
# regexes
#
re_int   = r"[0-9]+"
re_real  = r"[0-9]+(\.[0-9]*)?"
re_ident = r"[A-Za-z]+[A-Za-z0-9]*"


#
# [ regex parser, token id, converter function from lvalue string to lvalue ]
#
token_regexes = Any[
	[ re_int, ids.tok_int_id, str -> parse(Int, str) ],
	[ re_real, ids.tok_real_id, str -> parse(Float64, str) ],
	[ re_ident, ids.tok_ident_id, str -> str ],
]


#
# get the possibly matching tokens in a string
#
function get_matches(str :: AbstractString)
	matches = []

	# get all possible matches
	for to_match in token_regexes
		the_match = match(to_match[1], str)
		if the_match !== nothing && the_match.offset == 1 #&& the_match.match == str
			lval = the_match.match

			push!(matches, Any[
				to_match[2],       # token id
				lval,              # token lvalue string
				to_match[3](lval)  # token lvalue
			])
		end
	end

	# sort by match length
	matches = sort!(matches, by = elem -> length(elem[2]), rev = true)
	return matches
end


#
# get the next token in a string
#
function get_token(str :: AbstractString)
	str = lstrip(str)
	if length(str) == 0
		return [ nothing, str ]
	end

	matches = get_matches(str)
	if length(matches) > 0
		return [
			Any[ matches[1][1], matches[1][3] ],  # match
			str[length(matches[1][2]) + 1 : end]  # remaining string
		]
	end

	if str[1] == '+' || str[1] == '-' ||
		str[1] == '*' || str[1] == '/' ||
		str[1] == '%' || str[1] == '^' ||
		str[1] == '(' || str[1] == ')' ||
		str[1] == ','
		return [ str[1], str[2 : end] ]
	end

	return [ nothing, str ]
end



#
# get all tokens in a string
#
function get_tokens(str :: AbstractString)
	tokens = []

	while true
		token, str = get_token(str)
		if token === nothing
			break
		end
		push!(tokens, token)
	end

	return tokens
end


end
