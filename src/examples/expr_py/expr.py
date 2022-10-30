#!/bin/env python3
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

sys.path.append(".")
sys.path.append("../../modules")

import lexer

from ids import *
from lalr1_py import parser
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
	sem_start_id: lambda expr : expr["val"],
	sem_brackets_id: lambda bracket_open, expr, bracket_close : expr["val"],

	# binary arithmetic operations
	sem_add_id: lambda expr1, op_plus, expr2 : expr1["val"] + expr2["val"],
	sem_sub_id: lambda expr1, op_minus, expr2 : expr1["val"] - expr2["val"],
	sem_mul_id: lambda expr1, op_mult, expr2 : expr1["val"] * expr2["val"],
	sem_div_id: lambda expr1, op_div, expr2 : expr1["val"] / expr2["val"],
	sem_mod_id: lambda expr1, op_mod, expr2 : expr1["val"] % expr2["val"],
	sem_pow_id: lambda expr1, op_pow, expr2 : expr1["val"] ** expr2["val"],

	# unary arithmetic operations
	sem_uadd_id: lambda unary_plus, expr : +expr["val"],
	sem_usub_id: lambda unary_minus, expr : -expr["val"],

	# function calls
	sem_call0_id: lambda ident, bracket_open, bracket_close :
		functab_0args[ident["val"]](),
	sem_call1_id: lambda ident, bracket_open, expr, bracket_close :
		functab_1arg[ident["val"]](expr["val"]),
	sem_call2_id: lambda ident, bracket_open, expr1, comma, expr2, bracket_close :
		functab_2args[ident["val"]](expr1["val"], expr2["val"]),

	# symbols
	sem_real_id: lambda sym_real : sym_real["val"],
	sem_int_id: lambda sym_int : sym_int["val"],
	sem_ident_id: lambda sym_ident : symtab[sym_ident["val"]],
}



#
# load tables from a json file and run parser
#
def main(args):
	try:
		while True:
			sys.stdout.write("> ")
			sys.stdout.flush()
			input_str = sys.stdin.readline().strip()
			if len(input_str) == 0:
				continue

			tablesfile = open("expr.json")
			tables = json.load(tablesfile)
			theparser = parser.Parser(tables)

			#theparser = expr_parser.Parser()

			theparser.semantics = semantics
			end_token = theparser.end_token

			input_tokens = lexer.get_tokens(input_str)
			input_tokens.append([end_token])
			theparser.input_tokens = input_tokens

			result = theparser.parse()
			if result != None:
				print(result["val"])
			else:
				print("Error while parsing.")
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
