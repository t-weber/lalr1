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

	raise IndexError("No id for table index {0}.".format(idx))
	return None


#
# table-based LR(1) parser
#
class Parser:
	def __init__(self, tables):
		# tables
		self.shift_tab = tables["shift"]["elems"]
		self.reduce_tab = tables["reduce"]["elems"]
		self.jump_tab = tables["jump"]["elems"]
		self.termidx_tab = tables["term_idx"]
		self.nontermidx_tab = tables["nonterm_idx"]
		self.semanticidx_tab = tables["semantic_idx"]
		self.numrhs_tab = tables["num_rhs_syms"]
		self.lhsidx_tab = tables["lhs_idx"]

		# special values
		self.acc_token = tables["consts"]["acc"]
		self.err_token = tables["consts"]["err"]
		self.end_token = tables["consts"]["end"]
		self.start_idx = tables["consts"]["start"]

		self.input_tokens = []
		self.semantics = None
		self.reset()


	def reset(self):
		self.input_index = -1
		self.lookahead = None
		self.lookahead_idx = None
		self.accepted = False
		self.states = [ self.start_idx ] # parser states
		self.symbols = [ ]               # symbol stack


	#
	# get the next lookahead token and its table index
	#
	def get_next_lookahead(self):
		self.input_index = self.input_index + 1
		tok = self.input_tokens[self.input_index]
		tok_lval = tok[1] if len(tok) > 1 else None
		self.lookahead = { "is_term" : True, "id" : tok[0], "val" : tok_lval }
		self.lookahead_idx = get_table_index(self.termidx_tab, self.lookahead["id"])


	#
	# push the current lookahead token onto the symbol stack
	# and get the next lookahead
	#
	def push_lookahead(self):
		self.symbols.append(self.lookahead)
		self.get_next_lookahead()


	#
	# reduce using a semantic rule
	#
	def apply_rule(self, rule_id, num_rhs, lhs_id):
		#print(f"Reducing {num_syms} symbols.")
		args = self.symbols[len(self.symbols) - num_rhs : len(self.symbols)]
		self.symbols = self.symbols[0 : len(self.symbols) - num_rhs]
		self.states = self.states[0 : len(self.states) - num_rhs]

		# apply semantic rule if available
		rule_ret = None
		if self.semantics != None and rule_id in self.semantics:
			#print(f"args: {args}")
			rule_ret = self.semantics[rule_id](*args)

		# push reduced nonterminal symbol
		self.symbols.append({ "is_term" : False, "id" : lhs_id, "val" : rule_ret })


	#
	# run LR(1) parser
	#
	def parse(self):
		self.reset()
		self.get_next_lookahead()

		while True:
			top_state = self.states[-1]
			new_state = self.shift_tab[top_state][self.lookahead_idx]
			rule_idx = self.reduce_tab[top_state][self.lookahead_idx]
			#print(f"States: {self.states},\nsymbols: {self.symbols},\nnew state: {new_state}, rule: {rule_idx}.\n")

			if new_state == self.err_token and rule_idx == self.err_token:
				raise RuntimeError("No shift or reduce action defined.")
			elif new_state != self.err_token and rule_idx != self.err_token:
				raise RuntimeError("Shift/reduce conflict.")
			elif rule_idx == self.acc_token:
				# accept
				#print("Accepting.")
				self.accepted = True
				return self.symbols[-1] if len(self.symbols) >= 1 else None

			if new_state != self.err_token:
				# shift
				self.states.append(new_state)
				self.push_lookahead()

			elif rule_idx != self.err_token:
				# reduce
				rule_id = get_table_id(self.semanticidx_tab, rule_idx)
				num_syms = self.numrhs_tab[rule_idx]
				lhs_idx = self.lhsidx_tab[rule_idx]
				lhs_id = get_table_id(self.nontermidx_tab, lhs_idx)

				self.apply_rule(rule_id, num_syms, lhs_id)
				top_state = self.states[-1]
				self.states.append(self.jump_tab[top_state][lhs_idx])

		# input not accepted
		self.accepted = False
		return None
