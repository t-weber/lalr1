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
import random

import lexer
import parser
#import expr_parser


#
# symbol table
#
symtab = {
	"pi" : math.pi,
}


#
# function tables
#
functab_0args = {
	"rand01" : lambda : random.uniform(0., 1.)
}

functab_1arg = {
	"sqrt" : math.sqrt,
	"sin" : math.sin,
	"cos" : math.cos,
	"tan" : math.tan,
	"asin" : math.asin,
	"acos" : math.acos,
	"atan" : math.atan,
}

functab_2args = {
	"pow" : math.pow,
	"atan2" : math.atan2,
	"log" : lambda b, x : math.log(x) / math.log(b)
}


#
# semantic rules, same as in expr_grammar.cpp
#
semantics = {
	0: lambda expr : expr["val"],

	# binary arithmetic operations
	1: lambda expr1, op_plus, expr2 : expr1["val"] + expr2["val"],
	2: lambda expr1, op_minus, expr2 : expr1["val"] - expr2["val"],
	3: lambda expr1, op_mult, expr2 : expr1["val"] * expr2["val"],
	4: lambda expr1, op_div, expr2 : expr1["val"] / expr2["val"],
	5: lambda expr1, op_mod, expr2 : expr1["val"] % expr2["val"],
	6: lambda expr1, op_pow, expr2 : expr1["val"] ** expr2["val"],

	7: lambda bracket_open, expr, bracket_close : expr["val"],

	# function calls
	8: lambda ident, bracket_open, bracket_close :
		functab_0args[ident["val"]](),
	9: lambda ident, bracket_open, expr, bracket_close :
		functab_1arg[ident["val"]](expr["val"]),
	10: lambda ident, bracket_open, expr1, comma, expr2, bracket_close :
		functab_2args[ident["val"]](expr1["val"], expr2["val"]),

	# symbols
	11: lambda sym_real : sym_real["val"],
	12: lambda sym_int : sym_int["val"],
	13: lambda sym_ident : symtab[sym_ident["val"]],

	# unary arithmetic operations
	14: lambda unary_minus, expr : -expr["val"],
	15: lambda unary_plus, expr : +expr["val"],
}


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

			#parser = expr_parser.Parser()
			#parser.input_tokens = input_tokens
			#parser.semantics = semantics
			#result = parser.parse()
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
