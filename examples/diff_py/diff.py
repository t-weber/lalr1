#!/bin/env python3
#
# differentiating expression parser
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date july 2023
# @license see 'LICENSE' file
#
# References:
#	- https://en.wikipedia.org/wiki/Automatic_differentiation
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

import sys
import json
import math
import random

sys.path.append(".")
sys.path.append("../../src/modules")
sys.path.append("../src/modules")

import lexer

from ids import *
from lalr1_py import parser
#import diff_parser



#
# symbol table
#
symtab = {
	"pi" : math.pi,
	"__diffvar__" : "x"  # default variable for differentiation
}



#
# function tables and their differentials
#
functab_0args = {
	"rand01" : lambda : random.uniform(0., 1.)
}

functab_0args_diff = {
	"rand01" : lambda : 0.
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

functab_1arg_diff = {
	"sqrt" : lambda x : 1. / (2.*math.sqrt(x)),
	"sin" : math.cos,
	"cos" : lambda x : -math.sin(x),
	"tan" : lambda x : 1. / math.cos(x)**2.,
	"asin" : lambda x : 1. / math.sqrt(1. - x**2.),
	"acos" : lambda x : -1. / math.sqrt(1. - x**2.),
	"atan" : lambda x : 1. / (1. + x**2.),
}

functab_2args = {
	"pow" : math.pow,
	"atan2" : math.atan2,
	"log" : lambda b, x : math.log(x) / math.log(b)
}

functab_2args_diff1 = {
	"pow" : lambda x, y : y * math.pow(x, y - 1.),
	"atan2" : None,  # TODO
	"log" : lambda b, x : - math.log(x) / (b * math.log(b)**2.),
}

functab_2args_diff2 = {
	"pow" : lambda x, y : math.log(x) * math.pow(x, y),
	"atan2" : None,  # TODO
	"log" : lambda b, x : 1. / (x * math.log(b)),
}



#
# semantic rules
#
def semantics_uminus(args, done, retval):
	if not done:
		return None

	arg = args[1]["val"][0]
	diffarg = args[1]["val"][1]

	return [ -arg, -diffarg ]


def semantics_add(args, done, retval):
	if not done:
		return None

	arg1 = args[0]["val"][0]
	arg2 = args[2]["val"][0]
	diffarg1 = args[0]["val"][1]
	diffarg2 = args[2]["val"][1]

	val = arg1 + arg2
	diffval = diffarg1 + diffarg2

	return [ val, diffval ]


def semantics_sub(args, done, retval):
	if not done:
		return None

	arg1 = args[0]["val"][0]
	arg2 = args[2]["val"][0]
	diffarg1 = args[0]["val"][1]
	diffarg2 = args[2]["val"][1]

	val = arg1 - arg2
	diffval = diffarg1 - diffarg2

	return [ val, diffval ]


def semantics_mul(args, done, retval):
	if not done:
		return None

	arg1 = args[0]["val"][0]
	arg2 = args[2]["val"][0]
	diffarg1 = args[0]["val"][1]
	diffarg2 = args[2]["val"][1]

	val = arg1 * arg2
	diffval = arg1*diffarg2 + arg2*diffarg1

	return [ val, diffval ]


def semantics_div(args, done, retval):
	if not done:
		return None

	arg1 = args[0]["val"][0]
	arg2 = args[2]["val"][0]
	diffarg1 = args[0]["val"][1]
	diffarg2 = args[2]["val"][1]

	val = arg1 / arg2
	diffval = (diffarg1*arg2 - arg1*diffarg2) / arg2**2.

	return [ val, diffval ]


def semantics_mod(args, done, retval):
	if not done:
		return None

	arg1 = args[0]["val"][0]
	arg2 = args[2]["val"][0]
	diffarg1 = args[0]["val"][1]
	diffarg2 = args[2]["val"][1]

	val = arg1 % arg2
	diffval = 0.  # TODO

	return [ val, diffval ]


def semantics_pow(args, done, retval):
	if not done:
		return None

	arg1 = args[0]["val"][0]
	arg2 = args[2]["val"][0]
	diffarg1 = args[0]["val"][1]
	diffarg2 = args[2]["val"][1]

	val = arg1 ** arg2
	diffval = diffarg1 * arg2 * arg1**(arg2 - 1.) + \
		diffarg2 * arg1**arg2 * math.log(arg1)

	return [ val, diffval ]


def semantics_sym(args, done, retval):
	if not done:
		return None

	val = args[0]["val"]
	return [ val, 0. ]


def semantics_ident(args, done, retval):
	if not done:
		return None

	ident = args[0]["val"]
	val = symtab[ident]
	diffval = 0.
	if ident == symtab["__diffvar__"]:
		diffval = 1.

	return [ val, diffval ]


def semantics_func0(args, done, retval):
	if not done:
		return None

	funcname = args[0]["val"]
	val = functab_0args[funcname]()
	diffval = functab_0args_diff[funcname]()

	return [ val, diffval ]


def semantics_func1(args, done, retval):
	if not done:
		return None

	funcname = args[0]["val"]
	arg1 = args[2]["val"][0]
	diffarg1 = args[2]["val"][1]

	val = functab_1arg[funcname](arg1)
	diffval = diffarg1 * functab_1arg_diff[funcname](arg1)

	return [ val, diffval ]


def semantics_func2(args, done, retval):
	if not done:
		return None

	funcname = args[0]["val"]
	arg1 = args[2]["val"][0]
	arg2 = args[4]["val"][0]
	diffarg1 = args[2]["val"][1]
	diffarg2 = args[4]["val"][1]

	val = functab_2args[funcname](arg1, arg2)
	diffval = diffarg1 * functab_2args_diff1[funcname](arg1, arg2) + \
		diffarg2 * functab_2args_diff2[funcname](arg1, arg2)

	return [ val, diffval ]


def semantics_diff(args, done, retval):
	if done:
		return args[4]["val"]
	else:
		if len(args) == 3:
			var = args[0]["val"]
			val = args[2]["val"][0]

			symtab[var] = val
			symtab["__diffvar__"] = var
			print("{} = {}".format(var, val))



#
# semantic rules table
#
semantics = {
	sem_start_id: lambda args, done, retval : args[0]["val"] if done else None,
	sem_brackets_id: lambda args, done, retval : args[1]["val"] if done else None,

	sem_diff_id: semantics_diff,

	# binary arithmetic operations
	sem_add_id: semantics_add,
	sem_sub_id: semantics_sub,
	sem_mul_id: semantics_mul,
	sem_div_id: semantics_div,
	sem_mod_id: semantics_mod,
	sem_pow_id: semantics_pow,

	# unary arithmetic operations
	sem_uadd_id: lambda args, done, retval : args[1]["val"] if done else None,
	sem_usub_id: semantics_uminus,

	# function calls
	sem_call0_id: semantics_func0,
	sem_call1_id: semantics_func1,
	sem_call2_id: semantics_func2,

	# symbols
	sem_real_id: semantics_sym,
	sem_int_id: semantics_sym,
	sem_ident_id: semantics_ident,
}



#
# load tables from a json file and run parser
#
def main(args):
	print("Enter expression to differentiate, e.g. x = 5, x^2.")

	try:
		while True:
			sys.stdout.write("> ")
			sys.stdout.flush()
			input_str = sys.stdin.readline().strip()
			if len(input_str) == 0:
				continue

			tablesfile = open("diff.json")
			tables = json.load(tablesfile)
			theparser = parser.Parser(tables)
			#theparser = diff_parser.Parser()

			theparser.debug = False
			theparser.use_partials = True

			theparser.semantics = semantics
			end_token = theparser.end_token

			input_tokens = lexer.get_tokens(input_str)
			input_tokens.append([end_token])
			theparser.input_tokens = input_tokens

			result = theparser.parse()
			if result != None:
				diffvar = symtab["__diffvar__"]
				val, diffval = result["val"]

				print("f({}) = {}".format(diffvar, val))
				print("∂f({0})/∂{0} = {1}".format(diffvar, diffval))
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
