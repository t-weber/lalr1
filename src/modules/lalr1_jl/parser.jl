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
using tables: t_idx, t_id, get_table_index, get_table_id


#
# table-based LR(1) parser
#
mutable struct Parser
	# tables
	shift_tab :: Vector{Vector{t_idx}}
	reduce_tab :: Vector{Vector{t_idx}}
	jump_tab :: Vector{Vector{t_idx}}
	termidx_tab :: Vector{Vector{Union{t_idx, String}}}
	nontermidx_tab :: Vector{Vector{Union{t_idx, String}}}
	semanticidx_tab :: Vector{Vector{t_idx}}
	numrhs_tab :: Vector{t_idx}
	lhsidx_tab :: Vector{t_idx}

	# partial rule tables
	part_term :: Vector{Vector{t_idx}}
	part_nonterm :: Vector{Vector{t_id}}
	part_term_len :: Vector{Vector{t_idx}}
	part_nonterm_len :: Vector{Vector{t_idx}}

	# special values
	acc_token :: t_idx
	err_token :: t_idx
	end_token :: t_idx
	start_idx :: t_idx

	# options
	use_partials :: Bool
	debug :: Bool

	input_tokens :: Vector{Any}
	semantics :: Dict{t_id, Function}

	input_idx :: t_idx           # current token
	lookahead_idx :: t_idx
	lookahead :: Dict{String, Any}

	states :: Vector{t_idx}      # parser states
	symbols :: Vector            # symbol stack

	accepted :: Bool             # parsing successful?

	active_rules :: Dict{t_id, Any}  # active partial rules
	cur_rule_handle :: t_idx     # global rule counter


	#
	# constructor
	#
	function Parser(tables)
		parser = new()

		# parsing tables
		parser.shift_tab = tables["shift"]["elems"]
		parser.reduce_tab = tables["reduce"]["elems"]
		parser.jump_tab = tables["jump"]["elems"]

		# index tables
		parser.termidx_tab = tables["indices"]["term_idx"]
		parser.nontermidx_tab = tables["indices"]["nonterm_idx"]
		parser.semanticidx_tab = tables["indices"]["semantic_idx"]
		parser.numrhs_tab = tables["indices"]["num_rhs_syms"]
		parser.lhsidx_tab = tables["indices"]["lhs_idx"]

		# partial rules tables
		parser.part_term = tables["partials_rule_term"]["elems"]
		parser.part_nonterm = tables["partials_rule_nonterm"]["elems"]
		parser.part_term_len = tables["partials_matchlen_term"]["elems"]
		parser.part_nonterm_len = tables["partials_matchlen_nonterm"]["elems"]

		# special values
		parser.acc_token = tables["consts"]["acc"]
		parser.err_token = tables["consts"]["err"]
		parser.end_token = tables["consts"]["end"]
		parser.start_idx = tables["consts"]["start"]

		parser.input_tokens = [ ]
		parser.semantics = Dict{t_id, Function}()

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
	parser.lookahead_idx = 0
	parser.lookahead = Dict{String, Any}()
	parser.states = [ parser.start_idx ]    # parser states
	parser.symbols = [ ]                    # symbol stack
	parser.accepted = false
	parser.active_rules = Dict{t_id, Any}() # active partial rules
	parser.cur_rule_handle = 0              # global rule counter
end


#
# get the next lookahead token and its table index
#
function get_next_lookahead(parser::Parser)
	parser.input_idx += 1
	tok = parser.input_tokens[parser.input_idx]
	tok_lval = length(tok) > 1 ? tok[2] : nothing

	parser.lookahead = Dict{String, Any}( "is_term" => true, "id" => tok[1], "val" => tok_lval )
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
function apply_rule(parser::Parser, rule_id :: t_id, num_rhs :: t_idx, lhs_id :: t_id)
	# remove fully reduced rule from active rule stack and get return value
	rule_ret = nothing
	handle = -1

	if parser.use_partials
		rulestack = get!(parser.active_rules, rule_id, nothing)

		if rulestack != nothing && length(rulestack) > 0
			active_rule = rulestack[end]
			rule_ret = active_rule["retval"]
			handle = active_rule["handle"]

			# pop active rule
			if length(rulestack) == 1
				delete!(parser.active_rules, rule_id)
			else
				pop!(rulestack)
				parser.active_rules[rule_id] = rulestack
			end
		end
	end

	if parser.debug
		@printf("Reducing %d symbols using rule %d (handle %d).\n",
			num_rhs, rule_id, handle)
	end

	# get num_rhs argument symbols
	args = parser.symbols[end - num_rhs + 1 : end]

	# pop num_rhs symbols and states
	deleteat!(parser.symbols, length(parser.symbols) - num_rhs + 1 : length(parser.symbols))
	deleteat!(parser.states, length(parser.states) - num_rhs + 1 : length(parser.states))

	# apply semantic rule if available
	if rule_id in keys(parser.semantics)
		rule_ret = parser.semantics[rule_id](args, true, rule_ret)
	end

	# push reduced nonterminal symbol
	push!(parser.symbols,
		Dict{String, Any}( "is_term" => false, "id" => lhs_id, "val" => rule_ret ))
end


#
# partially apply a semantic rule
#
function apply_partial_rule(parser::Parser, rule_id :: t_id, rule_len :: t_idx, before_shift :: Bool)
	arg_len = rule_len
	if before_shift
		# directly count the following lookahead terminal
		rule_len += 1
	end

	already_seen_active_rule = false
	insert_new_active_rule = false
	seen_tokens_old = -1

	rulestack = get!(parser.active_rules, rule_id, nothing)

	if rulestack != nothing
		if length(rulestack) > 0
			active_rule = rulestack[end]
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
			rulestack[end] = active_rule
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

		active_rule = Dict{String, Any}()
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
		if !(rule_id in keys(parser.semantics))
			return
		end
		active_rule = rulestack[end]

		# get arguments for semantic rule
		args = parser.symbols[end - arg_len + 1 : end]
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
			rulestack[end] = active_rule
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
		top_state = parser.states[end]
		new_state = parser.shift_tab[top_state + 1][parser.lookahead_idx]
		rule_idx = parser.reduce_tab[top_state + 1][parser.lookahead_idx]

		if parser.debug
			@printf("States: %s,\n", parser.states)
			@printf("  symbols: %s},\n", parser.symbols)
			@printf("  new state: %s, rule: %d.\n", new_state, rule_idx)
		end

		# errors
		if new_state == parser.err_token && rule_idx == parser.err_token
			throw(SystemError("No shift or reduce action defined."))
		elseif new_state != parser.err_token && rule_idx != parser.err_token
			throw(SystemError("Shift/reduce conflict."))

		# accept
		elseif rule_idx == parser.acc_token
			if parser.debug
				printstyled(stdout, "Accepting.\n", color=:green, bold=true)
			end
			parser.accepted = true
			return length(parser.symbols) >= 1 ? parser.symbols[end] : nothing
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
			top_state = parser.states[end]

			# partial rules
			if parser.use_partials && length(parser.symbols) > 0
				partial_idx = parser.part_nonterm[top_state + 1][lhs_idx + 1]

				if partial_idx != parser.err_token
					partial_id = get_table_id(parser.semanticidx_tab, partial_idx)
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
