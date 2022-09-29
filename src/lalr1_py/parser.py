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
# get the internal table index of a token or nonterminal id
#
def get_table_index(idx_tab, id):
	for [theid, theidx] in idx_tab:
		if theid == id:
			return theidx

	raise IndexError("No table index for id {0}.".format(id))
	return None


#
# run LR(1) parser
#
def lr1parse(tables, input_tokens, semantics):
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
	states = [ 0 ]  # parser states
	symbols = [ ]   # array of [ is_terminal, index, lvalue for terminals, ast for nonterminals ]

	input_index = 0
	cur_tok = input_tokens[input_index]
	cur_tok_idx = get_table_index(termidx_tab, cur_tok[0])

	while True:
		top_state = states[-1]
		new_state = shift_tab[top_state][cur_tok_idx]
		new_rule = reduce_tab[top_state][cur_tok_idx]
		print(f"States: {states},\nsymbols: {symbols},\nnew state: {new_state}, rule: {new_rule}.\n")

		if new_state == err_token and new_rule == err_token:
			raise RuntimeError("No shift or reduce action defined.")
		elif new_state != err_token and new_rule != err_token:
			raise RuntimeError("Shift/reduce conflict.")
		elif new_rule == acc_token:
			# accept
			print("Accepting.")
			top_sym = None
			if len(symbols) >= 1:
				top_sym = symbols[-1]
			return [ True, top_sym ]

		if new_state != err_token:
			# shift
			states.append(new_state)
			symbols.append([True] + cur_tok)

			input_index += 1
			cur_tok = input_tokens[input_index]
			cur_tok_idx = get_table_index(termidx_tab, cur_tok[0])

		elif new_rule != err_token:
			# reduce
			num_syms = numrhs_tab[new_rule]
			lhs_idx = get_table_index(nontermidx_tab, lhsidx_tab[new_rule])

			print(f"Reducing {num_syms} symbols.")

			args = symbols[len(symbols)-num_syms : len(symbols)]
			symbols = symbols[0 : len(symbols)-num_syms]
			states = states[0 : len(states)-num_syms]

			# apply semantic rule if available
			rule_ret = None
			if semantics != None and new_rule < len(semantics):
				rule_ret = semantics[new_rule](args)

			# push reduced nonterminal symbol
			symbols.append([False, lhs_idx, rule_ret])

			top_state = states[-1]
			states.append(jump_tab[top_state][lhs_idx])

	# input not accepted
	return [ False, None ]


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

		if not lr1parse(tables, input_tokens, []):
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
