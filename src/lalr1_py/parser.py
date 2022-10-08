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
# get the token or terminal id of an internal table index
#
def get_table_id(idx_tab, idx):
	for [theid, theidx] in idx_tab:
		if theidx == idx:
			return theid

	raise IndexError("No id for table index {0}.".format(idx))
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
	semanticidx_tab = tables["semantic_idx"]
	numrhs_tab = tables["num_rhs_syms"]
	lhsidx_tab = tables["lhs_idx"]

	# special values
	acc_token = tables["consts"]["acc"]
	err_token = tables["consts"]["err"]

	# stacks
	states = [ 0 ]  # parser states
	symbols = [ ]   # symbol stack

	input_index = 0
	lookahead = input_tokens[input_index]
	lookahead_idx = get_table_index(termidx_tab, lookahead[0])

	while True:
		top_state = states[-1]
		new_state = shift_tab[top_state][lookahead_idx]
		rule_idx = reduce_tab[top_state][lookahead_idx]
		#print(f"States: {states},\nsymbols: {symbols},\nnew state: {new_state}, rule: {rule_idx}.\n")

		if new_state == err_token and rule_idx == err_token:
			raise RuntimeError("No shift or reduce action defined.")
		elif new_state != err_token and rule_idx != err_token:
			raise RuntimeError("Shift/reduce conflict.")
		elif rule_idx == acc_token:
			# accept
			#print("Accepting.")
			return symbols[-1] if len(symbols) >= 1 else None

		if new_state != err_token:
			# shift
			states.append(new_state)
			sym_lval = lookahead[1] if len(lookahead) > 1 else None
			symbols.append({ "is_term" : True, "id" : lookahead[0], "val" : sym_lval })

			input_index += 1
			lookahead = input_tokens[input_index]
			lookahead_idx = get_table_index(termidx_tab, lookahead[0])

		elif rule_idx != err_token:
			rule_id = get_table_id(semanticidx_tab, rule_idx)

			# reduce
			num_syms = numrhs_tab[rule_idx]
			lhs_idx = lhsidx_tab[rule_idx]
			lhs_id = get_table_id(nontermidx_tab, lhs_idx)

			#print(f"Reducing {num_syms} symbols.")
			args = symbols[len(symbols)-num_syms : len(symbols)]
			symbols = symbols[0 : len(symbols)-num_syms]
			states = states[0 : len(states)-num_syms]

			# apply semantic rule if available
			rule_ret = None
			if semantics != None and rule_id in semantics:
				#print(f"args: {args}")
				rule_ret = semantics[rule_id](*args)

			# push reduced nonterminal symbol
			symbols.append({ "is_term" : False, "id" : lhs_id, "val" : rule_ret })

			top_state = states[-1]
			states.append(jump_tab[top_state][lhs_idx])

	# input not accepted
	return None
