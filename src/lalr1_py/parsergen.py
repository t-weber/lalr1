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
	#if id == end_token:
	#	return "end_id"
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
	numrhs_tab = tables["num_rhs_syms"]
	lhsidx_tab = tables["lhs_idx"]

	# special values
	acc_token = tables["consts"]["acc"]
	err_token = tables["consts"]["err"]
	end_token = tables["consts"]["end"]

	#id_to_str = lambda id : str(id) if id!=end_token else "end"

	num_states = len(shift_tab)
	if num_states == 0:
		return False
	num_terms = len(shift_tab[0])
	num_nonterms = len(jump_tab[0])

	outfile = open(outfile_name, mode="wt")

	print("#\n# Parser created using liblalr1 by Tobias Weber, 2020-2022.\n" \
		"# DOI: https://doi.org/10.5281/zenodo.6987396\n#\n",
		file=outfile)

	print(f"end_id = {end_token}", file=outfile)
	print("lookahead = None", file=outfile)
	print("dist_to_jump = 0", file=outfile)
	print("accepted = False", file=outfile)
	print("symbols = []\n", file=outfile)

	print("def push_lookahead():", file=outfile)
	print("\tpass\n", file=outfile)

	print("def apply_rule(rule_idx, num_rhs):", file=outfile)
	print("\tdist_to_jump = num_rhs\n", file=outfile)

	for state_idx in range(num_states):
		print(f"def state_{state_idx}():", file=outfile)

		if has_table_entry(shift_tab, state_idx, err_token):
			print("\tnext_state = None", file=outfile)
		print("\tmatch lookahead[\"id\"]:", file=outfile)

		rules_term_id = {}
		acc_term_id = []
		for term_idx in range(num_terms):
			term_id = get_table_id(termidx_tab, term_idx)

			newstate_idx = shift_tab[state_idx][term_idx]
			rule_idx = reduce_tab[state_idx][term_idx]

			if newstate_idx != err_token:
				term_id_str = id_to_str(term_id, end_token)
				print(f"\t\tcase {term_id_str}: # index={term_idx}", file=outfile)
				print(f"\t\t\tnext_state = state_{newstate_idx}", file=outfile)

			elif rule_idx != err_token:
				if rule_idx == acc_token:
					acc_term_id.append(term_id)
				else:
					if not rule_idx in rules_term_id:
						rules_term_id[rule_idx] = []
					rules_term_id[rule_idx].append(term_id)

		for [rule_idx, term_id] in rules_term_id.items():
			print("\t\tcase {0}: # indices={1}".format(
				" | ".join(id_to_str(id, end_token) \
					for id in term_id),
				" ".join(str(get_table_index(termidx_tab, id)) \
					for id in term_id)),
				file=outfile)
			num_rhs = numrhs_tab[rule_idx]
			print(f"\t\t\tapply_rule({rule_idx}, {num_rhs})", file=outfile)

		if len(acc_term_id) > 0:
			print("\t\tcase {0}: # indices={1}".format(
				" | ".join(id_to_str(id, end_token) \
					for id in acc_term_id),
				" ".join(str(get_table_index(termidx_tab, id)) \
					for id in acc_term_id)),
				file=outfile)
			print(f"\t\t\taccept = True", file=outfile)

		print("\t\tcase _:", file=outfile)
		print(f"\t\t\traise RuntimeError(\"Invalid transition from state {state_idx}.\")", file=outfile)

		if has_table_entry(shift_tab, state_idx, err_token):
			print("\tif next_state != None:", file=outfile)
			print("\t\tpush_lookahead()", file=outfile)
			print("\t\tnext_state()", file=outfile)

		if has_table_entry(jump_tab, state_idx, err_token):
			print("\twhile dist_to_jump == 0 and len(symbols) > 0 and not accepted:", file=outfile)
			print("\t\ttopsym = symbols[-1]", file=outfile)
			print("\t\tif topsym[\"is_term\"]:", file=outfile)
			print("\t\t\tbreak", file=outfile)

			print("\t\tmatch topsym[\"id\"]:", file=outfile)
			for nonterm_idx in range(num_nonterms):
				nonterm_id = get_table_id(nontermidx_tab, nonterm_idx)
				jump_state_idx = jump_tab[state_idx][nonterm_idx]
				if jump_state_idx != err_token:
					print(f"\t\t\tcase {nonterm_id}: # index={nonterm_idx}", file=outfile)
					print(f"\t\t\t\tstate_{jump_state_idx}()", file=outfile)
		print("\t\t\tcase _:", file=outfile)
		print(f"\t\t\t\traise RuntimeError(\"Invalid transition from state {state_idx}.\")", file=outfile)

		print("\tdist_to_jump = dist_to_jump - 1", file=outfile)
		print("", file=outfile)

	return True


def main(args):
	if len(sys.argv) <= 1:
		print("Please give a LR(1) table file.", file=sys.stderr)
		return -1

	tablesfile_name = sys.argv[1]
	outfile_name = os.path.splitext(tablesfile_name)[0] + "_parser.py"

	try:
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
	except IndexError as err:
		print(f"Index error: {str(err)}", file=sys.stderr)
		return -1
	except BaseException as err:
		print(err, file=sys.stderr)
		return -1

	return 0


if __name__ == "__main__":
	main(sys.argv)
