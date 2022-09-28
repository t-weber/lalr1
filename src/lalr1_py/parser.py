#
# lalr(1) parser
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


#
# get the internal table index of a token id
#
def get_terminal_index(termidx_tab, token_id):
	for [term_id, term_idx] in termidx_tab:
		if term_id == token_id:
			return term_idx

	raise IndexError("No table index for terminal id {0}.".format(token_id))
	return None


#
# get the internal table index of a nonterminal id
#
def get_nonterminal_index(nontermidx_tab, id):
	for [nonterm_id, nonterm_idx] in nontermidx_tab:
		if nonterm_id == id:
			return nonterm_idx

	raise IndexError("No table index for non-terminal id {0}.".format(id))
	return None


#
# run LR(1) parser
#
def lr1parse(tables, input_tokens):
	# tables
	shift_tab = tables["shift"]["elems"]
	reduce_tab = tables["reduce"]["elems"]
	jump_tab = tables["jump"]["elems"]
	termidx_tab = tables["term_idx"]
	nontermidx_tab = tables["nonterm_idx"]
	numrhs_tab = tables["num_rhs_syms"]
	lhsidx_tab = tables["lhs_idx"]

	# special values
	end_token = tables["consts"]["end"]
	acc_token = tables["consts"]["acc"]
	err_token = tables["consts"]["err"]

	# stacks
	states = [ 0 ]
	symbols = []

	input_index = 0
	cur_tok = input_tokens[input_index]
	cur_tok_idx = get_terminal_index(termidx_tab, cur_tok[0])

	while True:
		top_state = states[-1]
		new_state = shift_tab[top_state][cur_tok_idx]
		new_rule = reduce_tab[top_state][cur_tok_idx]
		print(f"States: {states}, new state: {new_state}, rule: {new_rule}.")

		if new_state == err_token and new_rule == err_token:
			raise RuntimeError("No shift or reduce action defined.")
		elif new_state != err_token and new_rule != err_token:
			raise RuntimeError("Shift/reduce conflict.")
		elif new_rule == acc_token:
			# TODO: accept
			print("Accepting.")
			return True

		if new_state != err_token:
			# shift
			states.append(new_state)
			symbols.append([True] + cur_tok)

			input_index += 1
			cur_tok = input_tokens[input_index]
			cur_tok_idx = get_terminal_index(termidx_tab, cur_tok[0])

		elif new_rule != err_token:
			# reduce
			num_syms = numrhs_tab[new_rule]
			lhs_idx = get_nonterminal_index(nontermidx_tab, lhsidx_tab[new_rule])

			print(f"Reducing {num_syms} symbols.")

			args = symbols[len(symbols)-num_syms : len(symbols)]
			symbols = symbols[0 : len(symbols)-num_syms]
			states = states[0 : len(states)-num_syms]

			# TODO: apply semantic rule
			rule_ret = None
			symbols.append([False, lhs_idx, rule_ret])

			top_state = states[-1]
			states.append(jump_tab[top_state][lhs_idx])

	return True


#
# load tables from a json file and run parser
#
def main(args):
	if len(sys.argv) <= 1:
		print("Please give a LR(1) table json file.")
		return 0

	try:
		infile_name = sys.argv[1]
		infile = open(infile_name)

		tables = json.load(infile)
		end_token = tables["consts"]["end"]
		#print(tables["infos"])

		input_tokens = [ [1001, 1], ["+"], [1001, 2], ["*"], [1001, 3], [end_token] ]

		if not lr1parse(tables, input_tokens):
			print("Error while parsing.")
			return -1


	except FileNotFoundError as err:
		print(f"Error: Could not open tables file \"{infile_name}\".")
		return -1
	except json.decoder.JSONDecodeError as err:
		print(f"Error: Tables file \"{infile_name}\" could not be decoded as json.")
		return -1
	except IndexError as err:
		print(f"Index error: {str(err)}")
		return -1
	except BaseException as err:
		print(arr)
		return -1

	return 0


if __name__ == "__main__":
	main(sys.argv)
