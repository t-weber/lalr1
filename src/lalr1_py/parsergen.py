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


#
# get the internal table index of a token or nonterminal id
#
def get_table_index(idx_tab, id):
	for [theid, theidx] in idx_tab:
		if theid == id:
			return theidx

	raise IndexError("No table index for id {0}.".format(id))
	return None


#
# get the token or terminal id of an internal table index
#
def get_table_id(idx_tab, idx):
	for [theid, theidx] in idx_tab:
		if theidx == idx:
			return theid

	raise IndexError("No id for table index {0}.".format(id))
	return None


def has_table_entry(tab, idx1, err_token):
	row = tab[idx1]
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


def create_parser(tables, outfile_name):
	# tables
	shift_tab = tables["shift"]["elems"]
	reduce_tab = tables["reduce"]["elems"]
	jump_tab = tables["jump"]["elems"]
	termidx_tab = tables["term_idx"]
	nontermidx_tab = tables["nonterm_idx"]
	semanticidx_tab = tables["semantic_idx"]
	numrhs_tab = tables["num_rhs_syms"]
	lhsidx_tab = tables["lhs_idx"]

	# special values
	acc_token = tables["consts"]["acc"]
	err_token = tables["consts"]["err"]
	end_token = tables["consts"]["end"]

	num_states = len(shift_tab)
	if num_states == 0:
		return False
	num_terms = len(shift_tab[0])
	num_nonterms = len(jump_tab[0])

	outfile = open(outfile_name, mode="wt")

	print("#\n# Parser created using liblalr1 by Tobias Weber, 2020-2022.\n" \
		"# DOI: https://doi.org/10.5281/zenodo.6987396\n#\n",
		file=outfile)

	print("class Parser:", file=outfile)

	# constructor
	print("\tdef __init__(self):", file=outfile)
	#print(f"\t\tself.end_id = 0x{end_token:x}", file=outfile)
	print("\t\tself.input_tokens = []", file=outfile)
	print("\t\tself.semantics = None", file=outfile)
	print("\t\tself.reset()\n", file=outfile)

	# reset function
	print("\tdef reset(self):", file=outfile)
	print("\t\tself.input_index = -1", file=outfile)
	print("\t\tself.lookahead = None", file=outfile)
	print("\t\tself.dist_to_jump = 0", file=outfile)
	print("\t\tself.accepted = False", file=outfile)
	print("\t\tself.symbols = []\n", file=outfile)

	# get_next_lookahead function
	print("\tdef get_next_lookahead(self):", file=outfile)
	print("\t\tself.input_index = self.input_index + 1", file=outfile)
	print("\t\ttok = self.input_tokens[self.input_index]", file=outfile)
	print("\t\ttok_lval = tok[1] if len(tok) > 1 else None", file=outfile)
	print("\t\tself.lookahead = { \"is_term\" : True, \"id\" : tok[0], \"val\" : tok_lval }\n", file=outfile)

	# push_lookahead function
	print("\tdef push_lookahead(self):", file=outfile)
	print("\t\tself.symbols.append(self.lookahead)", file=outfile)
	print("\t\tself.get_next_lookahead()\n", file=outfile)

	# apply_rule function
	print("\tdef apply_rule(self, rule_id, num_rhs, lhs_id):", file=outfile)
	print("\t\tself.dist_to_jump = num_rhs", file=outfile)
	print("\t\targs = self.symbols[len(self.symbols)-num_rhs : len(self.symbols)]", file=outfile)
	print("\t\tself.symbols = self.symbols[0 : len(self.symbols) - num_rhs]", file=outfile)
	print("\t\trule_ret = None", file=outfile)
	print("\t\tif self.semantics != None and rule_id in self.semantics:", file=outfile)
	print("\t\t\trule_ret = self.semantics[rule_id](*args)", file=outfile)
	print("\t\tself.symbols.append({ \"is_term\" : False, \"id\" : lhs_id, \"val\" : rule_ret })\n", file=outfile)

	# parse function
	print("\tdef parse(self):", file=outfile)
	print("\t\tself.reset()", file=outfile)
	print("\t\tself.get_next_lookahead()", file=outfile)
	print("\t\tself.state_0()", file=outfile)
	print("\t\tif len(self.symbols) < 1 or not self.accepted:", file=outfile)
	print("\t\t\treturn None", file=outfile)
	print("\t\treturn self.symbols[-1]\n", file=outfile)

	# closures
	for state_idx in range(num_states):
		print(f"\tdef state_{state_idx}(self):", file=outfile)

		has_shift_entry = has_table_entry(shift_tab, state_idx, err_token)
		has_jump_entry = has_table_entry(jump_tab, state_idx, err_token)

		if has_shift_entry:
			print("\t\tnext_state = None", file=outfile)
		print("\t\tmatch self.lookahead[\"id\"]:", file=outfile)

		rules_term_id = {}
		acc_term_id = []
		for term_idx in range(num_terms):
			term_id = get_table_id(termidx_tab, term_idx)

			newstate_idx = shift_tab[state_idx][term_idx]
			rule_idx = reduce_tab[state_idx][term_idx]

			if newstate_idx != err_token:
				term_id_str = id_to_str(term_id, end_token)
				print(f"\t\t\tcase {term_id_str}: # index: {term_idx}", file=outfile)
				print(f"\t\t\t\tnext_state = self.state_{newstate_idx}", file=outfile)

			elif rule_idx != err_token:
				if rule_idx == acc_token:
					acc_term_id.append(term_id)
				else:
					if not rule_idx in rules_term_id:
						rules_term_id[rule_idx] = []
					rules_term_id[rule_idx].append(term_id)

		for [rule_idx, term_id] in rules_term_id.items():
			print("\t\t\tcase {0}: # indices: {1}".format(
				" | ".join(id_to_str(id, end_token) \
					for id in term_id),
				" ".join(str(get_table_index(termidx_tab, id)) \
					for id in term_id)),
				file=outfile)
			num_rhs = numrhs_tab[rule_idx]
			lhs_id = get_table_id(nontermidx_tab, lhsidx_tab[rule_idx])
			rule_id = get_table_id(semanticidx_tab, rule_idx)
			print(f"\t\t\t\tself.apply_rule({rule_id}, {num_rhs}, {lhs_id})", file=outfile)

		if len(acc_term_id) > 0:
			print("\t\t\tcase {0}: # indices: {1}".format(
				" | ".join(id_to_str(id, end_token) \
					for id in acc_term_id),
				" ".join(str(get_table_index(termidx_tab, id)) \
					for id in acc_term_id)),
				file=outfile)
			print(f"\t\t\t\tself.accepted = True", file=outfile)

		print("\t\t\tcase _:", file=outfile)
		print(f"\t\t\t\traise RuntimeError(\"Invalid transition from state {state_idx}.\")", file=outfile)

		if has_shift_entry:
			print("\t\tif next_state != None:", file=outfile)
			print("\t\t\tself.push_lookahead()", file=outfile)
			print("\t\t\tnext_state()", file=outfile)

		if has_jump_entry:
			print("\t\twhile self.dist_to_jump == 0 and len(self.symbols) > 0 and not self.accepted:", file=outfile)
			print("\t\t\ttopsym = self.symbols[-1]", file=outfile)
			print("\t\t\tif topsym[\"is_term\"]:", file=outfile)
			print("\t\t\t\tbreak", file=outfile)

			print("\t\t\tmatch topsym[\"id\"]:", file=outfile)
			for nonterm_idx in range(num_nonterms):
				nonterm_id = get_table_id(nontermidx_tab, nonterm_idx)
				jump_state_idx = jump_tab[state_idx][nonterm_idx]
				if jump_state_idx != err_token:
					print(f"\t\t\t\tcase {nonterm_id}: # index: {nonterm_idx}", file=outfile)
					print(f"\t\t\t\t\tself.state_{jump_state_idx}()", file=outfile)
			print("\t\t\t\tcase _:", file=outfile)
			print(f"\t\t\t\t\traise RuntimeError(\"Invalid transition from state {state_idx}.\")", file=outfile)

		print("\t\tself.dist_to_jump = self.dist_to_jump - 1", file=outfile)
		print("", file=outfile)

	return True


def main(args):
	if len(sys.argv) <= 1:
		print("Please give a LR(1) table file.", file=sys.stderr)
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
