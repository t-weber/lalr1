#
# lalr(1) parser
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 8-sep-2024
# @license see 'LICENSE' file
#
# References:
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

module parser

using Printf


#
# get the internal table index of a token or nonterminal id
#
function get_table_index(idx_tab, id)
	for entry in idx_tab
		if entry[1] == Int(id)
			return entry[2] + 1
		end
	end

	throw(DomainError(id, "No table index for this id."))
	return nothing
end


#
# get the token or terminal id of an internal table index
#
function get_table_id(idx_tab, idx)
	for entry in idx_tab
		if entry[2] == idx
			return entry[1]
		end
	end

	throw(DomainError(idx, "No id for this table index."))
	return nothing
end


#
# table-based LR(1) parser
#
mutable struct Parser
	# tables
	shift_tab
	reduce_tab
	jump_tab
	termidx_tab
	nontermidx_tab
	semanticidx_tab
	numrhs_tab
	lhsidx_tab

	# partial rule tables
	part_term
	part_nonterm
	part_term_len
	part_nonterm_len

	# special values
	acc_token
	err_token
	end_token
	start_idx

	# options
	use_partials
	debug

	input_tokens
	semantics

	input_idx
	lookahead
	lookahead_idx
	accepted
	states
	symbols
	active_rules
	cur_rule_handle


	#
	# constructor
	#
	function Parser(tables)
		parser = new()

		# tables
		parser.shift_tab = tables["shift"]["elems"]
		parser.reduce_tab = tables["reduce"]["elems"]
		parser.jump_tab = tables["jump"]["elems"]
		parser.termidx_tab = tables["indices"]["term_idx"]
		parser.nontermidx_tab = tables["indices"]["nonterm_idx"]
		parser.semanticidx_tab = tables["indices"]["semantic_idx"]
		parser.numrhs_tab = tables["indices"]["num_rhs_syms"]
		parser.lhsidx_tab = tables["indices"]["lhs_idx"]

		# partial rule tables
		parser.part_term = tables["partials_rule_term"]["elems"]
		parser.part_nonterm = tables["partials_rule_nonterm"]["elems"]
		parser.part_term_len = tables["partials_matchlen_term"]["elems"]
		parser.part_nonterm_len = tables["partials_matchlen_nonterm"]["elems"]

		# special values
		parser.acc_token = tables["consts"]["acc"]
		parser.err_token = tables["consts"]["err"]
		parser.end_token = tables["consts"]["end"]
		parser.start_idx = tables["consts"]["start"]

		parser.input_tokens = []
		parser.semantics = nothing

		# options
		parser.use_partials = true
		parser.debug = false

		reset(parser)
		return parser
	end
end


#
# reset the parser
#
function reset(parser::Parser)
	parser.input_idx = 0
	parser.lookahead = nothing
	parser.lookahead_idx = nothing
	parser.accepted = false
	parser.states = [ parser.start_idx ]  # parser states
	parser.symbols = [ ]                  # symbol stack
	parser.active_rules = Dict()          # active partial rules
	parser.cur_rule_handle = 0            # global rule counter
end


#
# get the next lookahead token and its table index
#
function get_next_lookahead(parser::Parser)
	parser.input_idx += 1
	tok = parser.input_tokens[parser.input_idx]
	tok_lval = length(tok) > 1 ? tok[2] : nothing

	parser.lookahead = Dict( "is_term" => true, "id" => tok[1], "val" => tok_lval )
	parser.lookahead_idx = get_table_index(parser.termidx_tab, parser.lookahead["id"])
end


#
# push the current lookahead token onto the symbol stack
# and get the next lookahead
#
function push_lookahead(parser::Parser)
	push!(parser.symbols, parser.lookahead)
	get_next_lookahead(parser)
end


#
# reduce using a semantic rule
#
function apply_rule(parser::Parser, rule_id, num_rhs, lhs_id)
	# remove fully reduced rule from active rule stack and get return value
	rule_ret = nothing
	handle = -1

	if parser.use_partials && rule_id in parser.active_rules
		rulestack = parser.active_rules[rule_id]
		if rulestack != nothing && length(rulestack) > 0
			active_rule = rulestack[length(rulestack)]
			rule_ret = active_rule["retval"]
			handle = active_rule["handle"]

			# pop active rule
			rulestack = rulestack[1 : length(rulestack) - 1]
			parser.active_rules[rule_id] = rulestack
		end
	end

	if parser.debug
		@printf("Reducing %d symbols using rule %d (handle %d).\n",
			num_rhs, rule_id, handle)
	end

	# get argument symbols
	args = parser.symbols[length(parser.symbols) - num_rhs + 1 : length(parser.symbols)]

	# pop symbols and states
	parser.symbols = parser.symbols[1 : length(parser.symbols) - num_rhs]
	parser.states = parser.states[1 : length(parser.states) - num_rhs]

	# apply semantic rule if available
	if parser.semantics != nothing && rule_id in parser.semantics.keys
		rule_ret = parser.semantics[rule_id](args, true, rule_ret)
	end

	# push reduced nonterminal symbol
	push!(parser.symbols, Dict( "is_term" => false, "id" => lhs_id, "val" => rule_ret ))
end


#
# partially apply a semantic rule
#
function apply_partial_rule(parser::Parser, rule_id, rule_len, before_shift)
	arg_len = rule_len
	if before_shift
		# directly count the following lookahead terminal
		rule_len += 1
	end

	already_seen_active_rule = false
	insert_new_active_rule = false
	seen_tokens_old = -1

	rulestack = nothing
	try
		rulestack = parser.active_rules[rule_id]
	catch
	end

	if rulestack != nothing
		if length(rulestack) > 0
			active_rule = rulestack[length(rulestack)]
			seen_tokens_old = active_rule["seen_tokens"]

			if before_shift
				if active_rule["seen_tokens"] < rule_len
					active_rule["seen_tokens"] = rule_len
				else
					insert_new_active_rule = true
				end
			else  # before jump
				if active_rule["seen_tokens"] == rule_len
					already_seen_active_rule = true
				else
					active_rule["seen_tokens"] = rule_len
				end
			end

			# save changed values
			rulestack[length(rulestack)] = active_rule
			parser.active_rules[rule_id] = rulestack
		else
			# no active rule yet
			insert_new_active_rule = true
		end
	else
		# no active rule yet
		rulestack = []
		parser.active_rules[rule_id] = rulestack
		insert_new_active_rule = true
	end

	if insert_new_active_rule
		seen_tokens_old = -1

		active_rule = Dict()
		active_rule["seen_tokens"] = rule_len
		active_rule["retval"] = nothing
		active_rule["handle"] = parser.cur_rule_handle
		parser.cur_rule_handle += 1

		# save changed values
		push!(rulestack, active_rule)
		parser.active_rules[rule_id] = rulestack
	end

	if !already_seen_active_rule
		# partially apply semantic rule if available
		if parser.semantics == nothing || !(rule_id in parser.semantics)
			return
		end
		active_rule = rulestack[length(rulestack)]

		# get arguments for semantic rule
		args = parser.symbols[length(parser.symbols) - arg_len + 1 : length(parser.symbols)]
		save_back = false

		if !before_shift || seen_tokens_old < rule_len - 1
			# run the semantic rule
			if parser.debug
				handle = active_rule["handle"]
				@printf("Applying partial rule %d with length %d (handle %d). Before shift: %d.\n",
					rule_id, arg_len, handle, before_shift)
			end

			active_rule["retval"] = parser.semantics[rule_id](
				args, false, active_rule["retval"])
			save_back = true
		end

		if before_shift
			if parser.debug
				handle = active_rule["handle"]
				@printf("Applying partial rule %d with length %d (handle %d). Before shift: %d.\n",
					rule_id, rule_len, handle, before_shift)
			end

			# run the semantic rule again
			push!(args, parser.lookahead)
			active_rule["retval"] = parser.semantics[rule_id](
				args, false, active_rule["retval"])
			save_back = true
		end

		# save changed values
		if save_back
			rulestack[length(rulestack)] = active_rule
			parser.active_rules[rule_id] = rulestack
		end
	end
end


#
# run LR(1) parser
#
function parse(parser::Parser)
	reset(parser)
	get_next_lookahead(parser)

	while true
		top_state = parser.states[length(parser.states)]
		new_state = parser.shift_tab[top_state + 1][parser.lookahead_idx]
		rule_idx = parser.reduce_tab[top_state + 1][parser.lookahead_idx]

		if parser.debug
			@printf("States: %s,\n", parser.states)
			@printf("symbols: %s},\n", parser.symbols)
			@printf("new state: %s, rule: %d.\n", new_state, rule_idx)
		end

		# errors
		if new_state == parser.err_token && rule_idx == parser.err_token
			throw(SystemError("No shift or reduce action defined."))
		elseif new_state != parser.err_token && rule_idx != parser.err_token
			throw(SystemError("Shift/reduce conflict."))

		# accept
		elseif rule_idx == parser.acc_token
			if parser.debug
				println("Accepting.")
			end
			parser.accepted = true
			return length(parser.symbols) >= 1 ? parser.symbols[length(parser.symbols)] : nothing
		end

		# shift
		if new_state != parser.err_token
			# partial rules
			if parser.use_partials
				partial_idx = parser.part_term[top_state + 1][parser.lookahead_idx]
				if partial_idx != parser.err_token
					partial_id = get_table_id(parser.semanticidx_tab, partial_idx)
					partial_len = parser.part_term_len[top_state + 1][parser.lookahead_idx]
					apply_partial_rule(parser, partial_id, partial_len, true)
				end
			end

			push!(parser.states, new_state)
			push_lookahead(parser)

		# reduce
		elseif rule_idx != parser.err_token
			rule_id = get_table_id(parser.semanticidx_tab, rule_idx)
			num_syms = parser.numrhs_tab[rule_idx + 1]
			lhs_idx = parser.lhsidx_tab[rule_idx + 1]
			lhs_id = get_table_id(parser.nontermidx_tab, lhs_idx)

			apply_rule(parser, rule_id, num_syms, lhs_id)
			top_state = parser.states[length(parser.states)]

			# partial rules
			if parser.use_partials && length(parser.symbols) > 0
				partial_idx = parser.part_nonterm[top_state + 1][lhs_idx + 1]

				if partial_idx != parser.err_token
					partial_id = get_table_id(parser.semanticidx_tab, partial_idx + 1)
					partial_len = parser.part_nonterm_len[top_state + 1][lhs_idx + 1]
					apply_partial_rule(parser, partial_id, partial_len, false)
				end
			end

			push!(parser.states, parser.jump_tab[top_state + 1][lhs_idx + 1])
		end
	end

	# input not accepted
	parser.accepted = false
	return nothing
end

end
