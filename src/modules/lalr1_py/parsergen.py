#!/bin/env python3
#
# generates a recursive ascent parser from LR(1) parser tables
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 2-oct-2022
# @license see 'LICENSE' file
#
# References:
#	- https://doi.org/10.1016/0020-0190(88)90061-0
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

import sys
import os
import json


g_gen_partials = True  # generate code for partial matches


#
# get the internal table index of a token or nonterminal id
#
def get_table_index(idx_tab, id):
	for entry in idx_tab:
		if entry[0] == id:
			return entry[1]

	raise IndexError("No table index for id {0}.".format(id))
	return None


#
# get the token or terminal id of an internal table index
#
def get_table_id(idx_tab, idx):
	for entry in idx_tab:
		if entry[1] == idx:
			return entry[0]

	raise IndexError("No id for table index {0}.".format(id))
	return None


#
# get the token or terminal string id of an internal table index
#
def get_table_strid(idx_tab, idx):
	for entry in idx_tab:
		if entry[1] == idx:
			return entry[2]

	raise IndexError("No string id for table index {0}.".format(id))
	return None


def has_table_entry(row, err_token):
	for elem in row:
		if elem != err_token:
			return True
	return False


def id_to_str(id, end_token):
	if id == end_token and id >= 0:
		return f"0x{id:x}"
		#return "end_id"
	if isinstance(id, str):
		return "\"" + str(id) + "\""
	return str(id)


def write_parser_class(tables, outfile):
	end_token = tables["consts"]["end"]
	start_idx = tables["consts"]["start"]

	# shortcut for writing to the file
	def pr(str): print(str, file = outfile)

	pr("#\n# Parser created using liblalr1 by Tobias Weber, 2020-2022.\n" \
		"# DOI: https://doi.org/10.5281/zenodo.6987396\n#\n")

	pr("class Parser:")

	# constructor
	pr("\tdef __init__(self):")
	if end_token < 0:
		pr(f"\t\tself.end_token = {end_token}")
	else:
		pr(f"\t\tself.end_token = 0x{end_token:x}")
	pr("\t\tself.input_tokens = []")
	pr("\t\tself.semantics = None")
	pr("\t\tself.debug = False")
	if g_gen_partials:
		pr("\t\tself.use_partials = True")
	pr("\t\tself.reset()\n")

	# reset function
	pr("""	def reset(self):
		self.input_index = -1
		self.lookahead = None
		self.dist_to_jump = 0
		self.accepted = False
		self.symbols = []""")
	if g_gen_partials:
		pr("\t\tself.active_rules = { }")
		pr("\t\tself.cur_rule_handle = 0\n")

	# get_next_lookahead function
	pr("""	def get_next_lookahead(self):
		self.input_index = self.input_index + 1
		tok = self.input_tokens[self.input_index]
		tok_lval = tok[1] if len(tok) > 1 else None
		self.lookahead = { \"is_term\" : True, \"id\" : tok[0], \"val\" : tok_lval }
""")

	# push_lookahead function
	pr("""	def push_lookahead(self):
		self.symbols.append(self.lookahead)
		self.get_next_lookahead()
""")

	# apply_rule function
	pr("""	def apply_rule(self, rule_id, num_rhs, lhs_id):
		rule_ret = None""")
	if g_gen_partials:
		pr("""		handle = -1
		if self.use_partials and rule_id in self.active_rules:
			rulestack = self.active_rules[rule_id]
			if rulestack != None and len(rulestack) > 0:
				active_rule = rulestack[len(rulestack) - 1]
				rule_ret = active_rule["retval"]
				handle = active_rule["handle"]
				rulestack = rulestack[0 : len(rulestack) - 1]
				self.active_rules[rule_id] = rulestack
		if self.debug:
			print(f"Reducing {num_rhs} symbols using rule {rule_id} (handle {handle}).")""")
	else:
		pr("""		if self.debug:
			print(f"Reducing {num_rhs} symbols using rule {rule_id}.")""")
	pr("""		self.dist_to_jump = num_rhs
		args = self.symbols[len(self.symbols) - num_rhs : len(self.symbols)]
		self.symbols = self.symbols[0 : len(self.symbols) - num_rhs]
		if self.semantics != None and rule_id in self.semantics:
			rule_ret = self.semantics[rule_id](args, True, None)
		self.symbols.append({ \"is_term\" : False, \"id\" : lhs_id, \"val\" : rule_ret })
""")

	# parse function
	pr("\tdef parse(self):")
	pr("\t\tself.reset()")
	pr("\t\tself.get_next_lookahead()")
	pr(f"\t\tself.state_{start_idx}()")
	pr("\t\tif len(self.symbols) < 1 or not self.accepted:")
	pr("\t\t\treturn None")
	pr("\t\treturn self.symbols[-1]\n")


def write_closure(tables, state_idx, outfile):
	# tables
	shift_tab = tables["shift"]["elems"][state_idx]
	reduce_tab = tables["reduce"]["elems"][state_idx]
	jump_tab = tables["jump"]["elems"][state_idx]
	termidx_tab = tables["term_idx"]
	nontermidx_tab = tables["nonterm_idx"]
	semanticidx_tab = tables["semantic_idx"]
	numrhs_tab = tables["num_rhs_syms"]
	lhsidx_tab = tables["lhs_idx"]

	# partial rule tables
	#part_term = tables["partials_rule_term"]["elems"]
	#part_nonterm = tables["partials_rule_nonterm"]["elems"]
	#part_term_len = tables["partials_matchlen_term"]["elems"]
	#part_nonterm_len = tables["partials_matchlen_nonterm"]["elems"]

	# special values
	acc_token = tables["consts"]["acc"]
	err_token = tables["consts"]["err"]
	end_token = tables["consts"]["end"]

	num_terms = len(shift_tab)
	num_nonterms = len(jump_tab)

	# shortcut for writing to the file
	def pr(str): print(str, file = outfile)

	pr(f"\tdef state_{state_idx}(self):")

	has_shift_entry = has_table_entry(shift_tab, err_token)
	has_jump_entry = has_table_entry(jump_tab, err_token)

	if has_shift_entry:
		pr("\t\tnext_state = None")
	pr("\t\tmatch self.lookahead[\"id\"]:")

	rules_term_id = {}
	acc_term_id = []
	for term_idx in range(num_terms):
		term_id = get_table_id(termidx_tab, term_idx)

		newstate_idx = shift_tab[term_idx]
		rule_idx = reduce_tab[term_idx]

		if newstate_idx != err_token:
			term_strid = get_table_strid(termidx_tab, term_idx)
			term_id_str = id_to_str(term_id, end_token)

			pr(f"\t\t\tcase {term_id_str}: # id: {term_strid}, index: {term_idx}")
			pr(f"\t\t\t\tnext_state = self.state_{newstate_idx}")

		elif rule_idx != err_token:
			if rule_idx == acc_token:
				acc_term_id.append(term_id)
			else:
				if not rule_idx in rules_term_id:
					rules_term_id[rule_idx] = []
				rules_term_id[rule_idx].append(term_id)

	for [rule_idx, term_id] in rules_term_id.items():
		pr("\t\t\tcase {0}: # indices: {1}".format(
			" | ".join(id_to_str(id, end_token) \
				for id in term_id),
			" ".join(str(get_table_index(termidx_tab, id)) \
				for id in term_id)))
		num_rhs = numrhs_tab[rule_idx]
		lhs_id = get_table_id(nontermidx_tab, lhsidx_tab[rule_idx])
		rule_id = get_table_id(semanticidx_tab, rule_idx)
		pr(f"\t\t\t\tself.apply_rule({rule_id}, {num_rhs}, {lhs_id})")

	if len(acc_term_id) > 0:
		pr("\t\t\tcase {0}: # indices: {1}".format(
			" | ".join(id_to_str(id, end_token) \
				for id in acc_term_id),
			" ".join(str(get_table_index(termidx_tab, id)) \
				for id in acc_term_id)))
		pr(f"\t\t\t\tself.accepted = True")

	pr("\t\t\tcase _:")
	pr(f"\t\t\t\traise RuntimeError(\"Invalid terminal transition from state {state_idx}.\")")

	if has_shift_entry:
		pr("\t\tif next_state != None:")
		pr("\t\t\tself.push_lookahead()")
		pr("\t\t\tnext_state()")

	if has_jump_entry:
		pr("\t\twhile self.dist_to_jump == 0 and len(self.symbols) > 0 and not self.accepted:")
		pr("\t\t\ttopsym = self.symbols[-1]")
		pr("\t\t\tif topsym[\"is_term\"]:")
		pr("\t\t\t\tbreak")

		pr("\t\t\tmatch topsym[\"id\"]:")
		for nonterm_idx in range(num_nonterms):
			nonterm_id = get_table_id(nontermidx_tab, nonterm_idx)
			jump_state_idx = jump_tab[nonterm_idx]

			if jump_state_idx != err_token:
				nonterm_strid = get_table_strid(nontermidx_tab, nonterm_idx)

				pr(f"\t\t\t\tcase {nonterm_id}: # id: {nonterm_strid} index: {nonterm_idx}")
				pr(f"\t\t\t\t\tself.state_{jump_state_idx}()")
		pr("\t\t\t\tcase _:")
		pr(f"\t\t\t\t\traise RuntimeError(\"Invalid nonterminal transition from state {state_idx}.\")")

	pr("\t\tself.dist_to_jump = self.dist_to_jump - 1")
	pr("")


def create_parser(tables, outfile_name):
	num_states = len(tables["shift"]["elems"])
	if num_states == 0:
		print("Error: No states defined.", file=sys.stderr)
		return False

	with open(outfile_name, mode="wt") as outfile:
		# basic parser class
		write_parser_class(tables, outfile)

		# closures
		for state_idx in range(num_states):
			write_closure(tables, state_idx, outfile)

	return True


def main(args):
	if len(sys.argv) <= 1:
		print("Please give a json parsing table file.", file=sys.stderr)
		return -1

	tablesfile_name = sys.argv[1]
	outfile_name = os.path.splitext(tablesfile_name)[0] + "_parser.py"

	try:
		print(f"Creating parser \"{tablesfile_name}\" -> \"{outfile_name}\".")

		tablesfile = open(tablesfile_name, mode="rt")
		tables = json.load(tablesfile)
		print(tables["infos"])

		if not create_parser(tables, outfile_name):
			print("Error: Failed creating parser.", file=sys.stderr)
			return -1
	except FileNotFoundError as err:
		print(f"Error: Could not open tables file \"{tablesfile_name}\".", file=sys.stderr)
		return -1
	except json.decoder.JSONDecodeError as err:
		print(f"Error: Tables file \"{tablesfile_name}\" could not be decoded as json.", file=sys.stderr)
		return -1

	return 0


if __name__ == "__main__":
	main(sys.argv)
