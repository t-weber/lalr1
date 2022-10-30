#!/bin/env python3
#
# generates the LALR(1) tables for an expression parser
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date oct-2022
# @license see 'LICENSE' file
#
# References:
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

import sys
sys.path.append(".")

import lalr1
from ids import *


# non-terminals
start = lalr1.make_nonterminal(nonterm_start, "start")
expr = lalr1.make_nonterminal(nonterm_expr, "expr")

# terminals
op_plus = lalr1.make_terminal('+', "+")
op_minus = lalr1.make_terminal('-', "-")
op_mult = lalr1.make_terminal('*', "*")
op_div = lalr1.make_terminal('/', "/")
op_mod = lalr1.make_terminal('%', "%")
op_pow = lalr1.make_terminal('^', "^")
bracket_open = lalr1.make_terminal('(', "(")
bracket_close = lalr1.make_terminal(')', ")")
comma = lalr1.make_terminal(',', ",")
sym_real = lalr1.make_terminal(tok_real_id, "real")
sym_int = lalr1.make_terminal(tok_int_id, "integer")
ident = lalr1.make_terminal(tok_ident_id, "ident")

# precedences and associativities
op_plus.SetPrecedence(70, 'l')
op_minus.SetPrecedence(70, 'l')
op_mult.SetPrecedence(80, 'l')
op_div.SetPrecedence(80, 'l')
op_mod.SetPrecedence(80, 'l')
op_pow.SetPrecedence(110, 'r')

# rules
# start -> expr
start.AddARule(lalr1.make_word([ expr ]), sem_start_id)
# expr -> expr + expr
expr.AddARule(lalr1.make_word([ expr, op_plus, expr ]), sem_add_id)
# expr -> expr - expr
expr.AddARule(lalr1.make_word([ expr, op_minus, expr ]), sem_sub_id)
# expr -> expr * expr
expr.AddARule(lalr1.make_word([ expr, op_mult, expr ]), sem_mul_id)
# expr -> expr / expr
expr.AddARule(lalr1.make_word([ expr, op_div, expr ]), sem_div_id)
# expr -> expr % expr
expr.AddARule(lalr1.make_word([ expr, op_mod, expr ]), sem_mod_id)
# expr -> expr ^ expr
expr.AddARule(lalr1.make_word([ expr, op_pow, expr ]), sem_pow_id)
# expr -> ( expr )
expr.AddARule(lalr1.make_word([ bracket_open, expr, bracket_close ]), sem_brackets_id)
# expr -> ident()
expr.AddARule(lalr1.make_word([ ident, bracket_open, bracket_close ]), sem_call0_id)
# expr -> ident(expr)
expr.AddARule(lalr1.make_word([ ident, bracket_open, expr, bracket_close ]), sem_call1_id)
# expr -> ident(expr, expr)
expr.AddARule(lalr1.make_word([ ident, bracket_open, expr, comma, expr, bracket_close ]), sem_call2_id)
# expr -> real symbol
expr.AddARule(lalr1.make_word([ sym_real ]), sem_real_id)
# expr -> int symbol
expr.AddARule(lalr1.make_word([ sym_int ]), sem_int_id)
# expr -> int symbol
expr.AddARule(lalr1.make_word([ ident ]), sem_ident_id)
# expr -> -expr
expr.AddARule(lalr1.make_word([ op_minus, expr ]), sem_usub_id)
# expr -> +expr
expr.AddARule(lalr1.make_word([ op_plus, expr ]), sem_uadd_id)

# collection
coll = lalr1.make_collection(start)
print("Calculating LALR(1) collection.")
coll.DoTransitions()

# table generation
tablesfile_name = "expr.json"
if coll.CreateParseTables():
	tables = lalr1.TableExporter(coll)
	tables.SaveParseTablesJSON(tablesfile_name)
	print(f"Created parsing tables \"{tablesfile_name}\".")
else:
	print(f"Could not create parsing tables \"{tablesfile_name}\".", file=sys.stderr)
