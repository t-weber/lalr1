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

import os
import sys
import math
import random
import time

sys.path.append(".")
sys.path.append("../../src/modules")
sys.path.append("../src/modules")

import lexer
from ids import *

debug = False
use_partials = False

# select parser backend
parsing_tables = "expr.json"

if os.path.isfile("expr_parser.py"):
	import expr_parser
	use_recasc = True
elif os.path.isfile(parsing_tables):
	import json
	from lalr1_py import parser
	use_recasc = False
else:
	print("No parsing tables found. Please generate them using ./tablegen.py.")
	print("Optionally create a recursive-ascent parser using ../../src/modules/lalr1_py/parsergen.py {}.".format(parsing_tables))
	exit(-1)


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
	sem_start_id: lambda args, done, retval : args[0]["val"] if done else None,
	sem_brackets_id: lambda args, done, retval : args[1]["val"] if done else None,

	# binary arithmetic operations
	sem_add_id: lambda args, done, retval : args[0]["val"] + args[2]["val"] if done else None,
	sem_sub_id: lambda args, done, retval : args[0]["val"] - args[2]["val"] if done else None,
	sem_mul_id: lambda args, done, retval : args[0]["val"] * args[2]["val"] if done else None,
	sem_div_id: lambda args, done, retval : args[0]["val"] / args[2]["val"] if done else None,
	sem_mod_id: lambda args, done, retval : args[0]["val"] % args[2]["val"] if done else None,
	sem_pow_id: lambda args, done, retval : args[0]["val"] ** args[2]["val"] if done else None,

	# unary arithmetic operations
	sem_uadd_id: lambda args, done, retval : +args[1]["val"] if done else None,
	sem_usub_id: lambda args, done, retval : -args[1]["val"] if done else None,

	# function calls
	sem_call0_id: lambda args, done, retval :
		functab_0args[args[0]["val"]]() if done else None,
	sem_call1_id: lambda args, done, retval :
		functab_1arg[args[0]["val"]](args[2]["val"]) if done else None,
	sem_call2_id: lambda args, done, retval :
		functab_2args[args[0]["val"]](args[2]["val"], args[4]["val"]) if done else None,

	# symbols
	sem_real_id: lambda args, done, retval : args[0]["val"] if done else None,
	sem_int_id: lambda args, done, retval : args[0]["val"] if done else None,
	sem_ident_id: lambda args, done, retval : symtab[args[0]["val"]] if done else None,
}



#
# load tables from a json file and run parser
#
def main(args):
	try:
		if use_recasc:
			print("Using recursive-ascent parser.")
			theparser = expr_parser.Parser()
		else:
			print("Using table-based parser.")
			tablesfile = open(parsing_tables)
			tables = json.load(tablesfile)
			theparser = parser.Parser(tables)

		theparser.debug = debug
		theparser.use_partials = use_partials

		theparser.semantics = semantics
		end_token = theparser.end_token

		while True:
			theparser.reset()

			sys.stdout.write("> ")
			sys.stdout.flush()
			input_str = sys.stdin.readline().strip()
			if len(input_str) == 0:
				continue
			elif input_str == "exit":
				break

			input_tokens = lexer.get_tokens(input_str)
			input_tokens.append([end_token])
			theparser.input_tokens = input_tokens

			if debug:
				print("Input tokens: %s." % str(input_tokens))

			tick = time.time()
			result = theparser.parse()
			elapsed_time = (time.time() - tick) * 1000.
			if result != None:
				print("%g\t\t[t = %.4g ms]" % (result["val"], elapsed_time))
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
