#
# expression parser
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date sep-2022
# @license see 'LICENSE' file
#
# References:
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

import sys
import json
import math

import lexer
import parser


#
# symbol table
#
symtab = {
	"pi" : math.pi
}


#
# semantic rules, same as in expr_grammar.cpp
#
semantics = [
	lambda expr : expr["val"],

	# binary arithmetic operations
	lambda expr1, op_plus, expr2 : expr1["val"] + expr2["val"],
	lambda expr1, op_minus, expr2 : expr1["val"] - expr2["val"],
	lambda expr1, op_mult, expr2 : expr1["val"] * expr2["val"],
	lambda expr1, op_div, expr2 : expr1["val"] / expr2["val"],
	lambda expr1, op_mod, expr2 : expr1["val"] % expr2["val"],
	lambda expr1, op_pow, expr2 : expr1["val"] ** expr2["val"],

	lambda bracket_open, expr, bracket_close : expr["val"],

	# function calls
	lambda ident, bracket_open, bracket_close : 0.,
	lambda ident, bracket_open, expr, bracket_close : 0.,
	lambda ident, bracket_open, expr1, expr2, bracket_close : 0.,

	# symbols
	lambda sym_real : sym_real["val"],
	lambda sym_int : sym_int["val"],
	lambda sym_ident : symtab[sym_ident["val"]],

	# unary arithmetic operations
	lambda unary_minus, expr : -expr["val"],
	lambda unary_plus, expr : +expr["val"],
]


#
# token ids, same as in lexer.h
#
real_id = 1000
int_id  = 1001


#
# load tables from a json file and run parser
#
def main(args):
	tablesfile_name = "expr.json"   # output from ./expr_parsergen
	if len(sys.argv) > 1:
		tablesfile_name = sys.argv[1]

	try:
		tablesfile = open(tablesfile_name)

		tables = json.load(tablesfile)
		end_token = tables["consts"]["end"]
		#print(tables["infos"])

		while True:
			sys.stdout.write("> ")
			sys.stdout.flush()
			input_str = sys.stdin.readline().strip()
			if len(input_str) == 0:
				continue

			input_tokens = lexer.get_tokens(input_str)
			input_tokens.append([end_token])

			result = parser.lr1_parse(tables, input_tokens, semantics)
			if result != None:
				print(result["val"])
			else:
				print("Error while parsing.")
				return -1

	except FileNotFoundError as err:
		print(f"Error: Could not open tables file \"{tablesfile_name}\".")
		return -1
	except json.decoder.JSONDecodeError as err:
		print(f"Error: Tables file \"{tablesfile_name}\" could not be decoded as json.")
		return -1
	except IndexError as err:
		print(f"Index error: {str(err)}")
		return -1
	except BaseException as err:
		print(err)
		return -1

	return 0


if __name__ == "__main__":
	main(sys.argv)
