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
def lr1_parse(tables, input_tokens, semantics):
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
		#print(f"States: {states},\nsymbols: {symbols},\nnew state: {new_state}, rule: {new_rule}.\n")

		if new_state == err_token and new_rule == err_token:
			raise RuntimeError("No shift or reduce action defined.")
		elif new_state != err_token and new_rule != err_token:
			raise RuntimeError("Shift/reduce conflict.")
		elif new_rule == acc_token:
			# accept
			#print("Accepting.")
			return symbols[-1] if len(symbols) >= 1 else None

		if new_state != err_token:
			# shift
			states.append(new_state)
			sym_lval = cur_tok[1] if len(cur_tok) > 1 else None
			symbols.append({ "is_term" : True, "id" : cur_tok[0], "val" : sym_lval })

			input_index += 1
			cur_tok = input_tokens[input_index]
			cur_tok_idx = get_table_index(termidx_tab, cur_tok[0])

		elif new_rule != err_token:
			# reduce
			num_syms = numrhs_tab[new_rule]
			lhs_idx = get_table_index(nontermidx_tab, lhsidx_tab[new_rule])

			#print(f"Reducing {num_syms} symbols.")
			args = symbols[len(symbols)-num_syms : len(symbols)]
			symbols = symbols[0 : len(symbols)-num_syms]
			states = states[0 : len(states)-num_syms]

			# apply semantic rule if available
			rule_ret = None
			if semantics != None and new_rule < len(semantics):
				#print(f"args: {args}")
				rule_ret = semantics[new_rule](*args)

			# push reduced nonterminal symbol
			symbols.append({ "is_term" : False, "id" : lhs_idx, "val" : rule_ret })

			top_state = states[-1]
			states.append(jump_tab[top_state][lhs_idx])

	# input not accepted
	return None
