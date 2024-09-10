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

from .tables import get_table_index, get_table_id


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

		# partial rule tables
		self.part_term = tables["partials_rule_term"]["elems"]
		self.part_nonterm = tables["partials_rule_nonterm"]["elems"]
		self.part_term_len = tables["partials_matchlen_term"]["elems"]
		self.part_nonterm_len = tables["partials_matchlen_nonterm"]["elems"]

		# special values
		self.acc_token = tables["consts"]["acc"]
		self.err_token = tables["consts"]["err"]
		self.end_token = tables["consts"]["end"]
		self.start_idx = tables["consts"]["start"]

		self.input_tokens = []
		self.semantics = None

		# options
		self.use_partials = True
		self.debug = False

		self.reset()


	#
	# reset the parser
	#
	def reset(self):
		self.input_index = -1
		self.lookahead = None
		self.lookahead_idx = None
		self.accepted = False
		self.states = [ self.start_idx ] # parser states
		self.symbols = [ ]               # symbol stack
		self.active_rules = { }          # active partial rules
		self.cur_rule_handle = 0         # global rule counter


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
		# remove fully reduced rule from active rule stack and get return value
		rule_ret = None
		handle = -1
		if self.use_partials and rule_id in self.active_rules:
			rulestack = self.active_rules[rule_id]
			if rulestack != None and len(rulestack) > 0:
				active_rule = rulestack[len(rulestack) - 1]
				rule_ret = active_rule["retval"]
				handle = active_rule["handle"]

				# pop active rule
				rulestack = rulestack[0 : len(rulestack) - 1]
				self.active_rules[rule_id] = rulestack

		if self.debug:
			print(f"Reducing {num_rhs} symbols using rule {rule_id} (handle {handle}).")

		# get argument symbols
		args = self.symbols[len(self.symbols) - num_rhs : len(self.symbols)]

		# pop symbols and states
		self.symbols = self.symbols[0 : len(self.symbols) - num_rhs]
		self.states = self.states[0 : len(self.states) - num_rhs]

		# apply semantic rule if available
		if self.semantics != None and rule_id in self.semantics:
			#print(f"args: {args}")
			rule_ret = self.semantics[rule_id](args, True, rule_ret)

		# push reduced nonterminal symbol
		self.symbols.append({ "is_term" : False, "id" : lhs_id, "val" : rule_ret })


	#
	# partially apply a semantic rule
	#
	def apply_partial_rule(self, rule_id, rule_len, before_shift):
		arg_len = rule_len
		if before_shift:
			# directly count the following lookahead terminal
			rule_len += 1

		already_seen_active_rule = False
		insert_new_active_rule = False
		seen_tokens_old = -1

		rulestack = None
		try:
			rulestack = self.active_rules[rule_id]
		except KeyError as err:
			pass
		if rulestack != None:
			if len(rulestack) > 0:
				active_rule = rulestack[len(rulestack) - 1]
				seen_tokens_old = active_rule["seen_tokens"]

				if before_shift:
					if active_rule["seen_tokens"] < rule_len:
						active_rule["seen_tokens"] = rule_len
					else:
						insert_new_active_rule = True
				else:  # before jump
					if active_rule["seen_tokens"] == rule_len:
						already_seen_active_rule = True
					else:
						active_rule["seen_tokens"] = rule_len

				# save changed values
				rulestack[len(rulestack) - 1] = active_rule
				self.active_rules[rule_id] = rulestack
			else:
				# no active rule yet
				insert_new_active_rule = True
		else:
			# no active rule yet
			rulestack = []
			self.active_rules[rule_id] = rulestack
			insert_new_active_rule = True

		if insert_new_active_rule:
			seen_tokens_old = -1

			active_rule = {}
			active_rule["seen_tokens"] = rule_len
			active_rule["retval"] = None
			active_rule["handle"] = self.cur_rule_handle
			self.cur_rule_handle += 1

			# save changed values
			rulestack.append(active_rule)
			self.active_rules[rule_id] = rulestack

		if not already_seen_active_rule:
			# partially apply semantic rule if available
			if self.semantics == None or rule_id not in self.semantics:
				return
			active_rule = rulestack[len(rulestack) - 1]

			# get arguments for semantic rule
			args = self.symbols[len(self.symbols) - arg_len : len(self.symbols)]
			save_back = False

			if not before_shift or seen_tokens_old < rule_len - 1:
				# run the semantic rule
				if self.debug:
					handle = active_rule["handle"]
					print(f"Applying partial rule {rule_id} with "
						f"length {arg_len} (handle {handle}). "
						f"Before shift: {before_shift}.")
				active_rule["retval"] = self.semantics[rule_id](
					args, False, active_rule["retval"])
				save_back = True

			if before_shift:
				if self.debug:
					handle = active_rule["handle"]
					print(f"Applying partial rule {rule_id} with "
						f"length {rule_len} (handle {handle}). "
						f"Before shift: {before_shift}.")

				# run the semantic rule again
				args.append(self.lookahead)
				active_rule["retval"] = self.semantics[rule_id](
					args, False, active_rule["retval"])
				save_back = True

			# save changed values
			if save_back:
				rulestack[len(rulestack) - 1] = active_rule
				self.active_rules[rule_id] = rulestack


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
			if self.debug:
				print(f"States: {self.states},\n"
					f"symbols: {self.symbols},\n"
					f"new state: {new_state}, "
					f"rule: {rule_idx}.\n")

			# errors
			if new_state == self.err_token and rule_idx == self.err_token:
				raise RuntimeError("No shift or reduce action defined.")
			elif new_state != self.err_token and rule_idx != self.err_token:
				raise RuntimeError("Shift/reduce conflict.")

			# accept
			elif rule_idx == self.acc_token:
				if self.debug:
					print("Accepting.")
				self.accepted = True
				return self.symbols[-1] if len(self.symbols) >= 1 else None

			# shift
			if new_state != self.err_token:
				# partial rules
				if self.use_partials:
					partial_idx = self.part_term[top_state][self.lookahead_idx]
					if partial_idx != self.err_token:
						partial_id = get_table_id(self.semanticidx_tab, partial_idx)
						partial_len = self.part_term_len[top_state][self.lookahead_idx]
						self.apply_partial_rule(partial_id, partial_len, True)

				self.states.append(new_state)
				self.push_lookahead()

			# reduce
			elif rule_idx != self.err_token:
				rule_id = get_table_id(self.semanticidx_tab, rule_idx)
				num_syms = self.numrhs_tab[rule_idx]
				lhs_idx = self.lhsidx_tab[rule_idx]
				lhs_id = get_table_id(self.nontermidx_tab, lhs_idx)

				self.apply_rule(rule_id, num_syms, lhs_id)
				top_state = self.states[-1]

				# partial rules
				if self.use_partials and len(self.symbols) > 0:
					partial_idx = self.part_nonterm[top_state][lhs_idx]
					if partial_idx != self.err_token:
						partial_id = get_table_id(self.semanticidx_tab, partial_idx)
						partial_len = self.part_nonterm_len[top_state][lhs_idx]
						self.apply_partial_rule(partial_id, partial_len, False)

				self.states.append(self.jump_tab[top_state][lhs_idx])

		# input not accepted
		self.accepted = False
		return None
