#
# generates a recursive ascent parser from LR(1) parser tables
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 8-sep-2024
# @license see 'LICENSE' file
#
# References:
#	- https://doi.org/10.1016/0020-0190(88)90061-0
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

using TOML
using Printf

module parsergen

using Printf

push!(LOAD_PATH, ".")
push!(LOAD_PATH, @__DIR__)

using tables: t_idx, t_id, get_table_index, get_table_id,
	get_table_strid, has_table_entry, id_to_str


function write_parser(tables::Dict{String, Any},
	outfile::IOStream, gen_partials::Bool = false)
	end_token :: t_idx = tables["consts"]["end"]
	start_idx :: t_idx = tables["consts"]["start"]

	# shortcut for writing to the file
	function pr(str :: AbstractString)
		write(outfile, str * "\n")
	end

	pr("using Printf\n")

	pr("const t_idx = Integer")
	pr("const t_id  = Integer\n")

	# parser struct
	pr("""
	mutable struct Parser
		end_token :: t_idx
		debug :: Bool
		input_tokens :: Vector{Any}
		input_idx :: t_idx
		semantics :: Dict{t_id, Function}
		lookahead :: Dict{String, Any}
		symbols :: Vector
		dist_to_jump :: Integer
		accepted :: Bool""")

	if gen_partials
		pr("\tuse_partials :: Bool")
		pr("\tactive_rules :: Dict{t_id, Any}")
		pr("\tcur_rule_handle :: t_idx")
	end

	# constructor
	pr("\n\tfunction Parser()")
	pr("\t\tparser = new()")
	if end_token < 0
		pr("\t\tparser.end_token = " * string(end_token))
	else
		pr("\t\tparser.end_token = 0x" * string(end_token, base = 16))
	end
	pr("\t\tparser.debug = false")
	pr("\t\tparser.input_tokens = [ ]")
	pr("\t\tparser.semantics = Dict{t_id, Function}()")
	if gen_partials  # partial rules
		pr("\t\tparser.use_partials = true")
	end
	pr("\t\treset(parser)")
	pr("\t\treturn parser")
	pr("\tend")

	pr("end\n") # struct

	# reset function
	pr("""
	function reset(parser::Parser)
		parser.input_idx = 0
		parser.lookahead = Dict{String, Any}()
		parser.symbols = [ ]
		parser.dist_to_jump = 0
		parser.accepted = false""")
		if gen_partials  # partial rules
			pr("\tparser.active_rules = Dict{t_id, Any}()")
			pr("\tparser.cur_rule_handle = 0\n")
		end
	pr("end\n")

	# get_next_lookahead function
	pr("""
	function get_next_lookahead(parser::Parser)
		parser.input_idx += 1
		tok = parser.input_tokens[parser.input_idx]
		tok_lval = length(tok) > 1 ? tok[2] : nothing
		parser.lookahead = Dict{String, Any}( "is_term" => true, "id" => tok[1], "val" => tok_lval )
	end
	""")

	# push_lookahead function
	pr("""
	function push_lookahead(parser::Parser)
		push!(parser.symbols, parser.lookahead)
		get_next_lookahead(parser)
	end
	""")

	# apply_partial_rule function
	if gen_partials
		pr("""
		function apply_partial_rule(parser::Parser, rule_id::t_id, rule_len::t_idx, before_shift::Bool)
			arg_len = rule_len
			if before_shift
				rule_len += 1
			end
			already_seen_active_rule = false
			insert_new_active_rule = false
			seen_tokens_old = -1
			rulestack = get!(parser.active_rules, rule_id, nothing)
			if rulestack !== nothing
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
					rulestack[end] = active_rule
					parser.active_rules[rule_id] = rulestack
				else
					insert_new_active_rule = true
				end
			else
				rulestack = [ ]
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
				push!(rulestack, active_rule)
				parser.active_rules[rule_id] = rulestack
			end
			if !already_seen_active_rule
				if !(rule_id in keys(parser.semantics))
					return
				end
				active_rule = rulestack[end]
				args = parser.symbols[end - arg_len + 1 : end]
				save_back = false
				if !before_shift || seen_tokens_old < rule_len - 1
					if parser.debug
						handle = active_rule["handle"]
						@printf("Applying partial rule %d with length %d (handle %d). Before shift: %d.\\n",
							rule_id, arg_len, handle, before_shift)
					end
					active_rule["retval"] = parser.semantics[rule_id](
						args, false, active_rule["retval"])
					save_back = true
				end

				if before_shift
					if parser.debug
						handle = active_rule["handle"]
						@printf("Applying partial rule %d with length %d (handle %d). Before shift: %d.\\n",
							rule_id, rule_len, handle, before_shift)
					end
					push!(args, parser.lookahead)
					active_rule["retval"] = parser.semantics[rule_id](
						args, false, active_rule["retval"])
					save_back = true
				end
				if save_back
					rulestack[end] = active_rule
					parser.active_rules[rule_id] = rulestack
				end
			end
		end
		""")
	end

	# apply_rule function
	pr("""
	function apply_rule(parser::Parser, rule_id::t_id, num_rhs::t_idx, lhs_id::t_id)
		rule_ret = nothing""")
	if gen_partials
	pr("""	handle = -1
		if parser.use_partials
			rulestack = get!(parser.active_rules, rule_id, nothing)
			if rulestack !== nothing && length(rulestack) > 0
				active_rule = rulestack[end]
				rule_ret = active_rule["retval"]
				handle = active_rule["handle"]
				if length(rulestack) == 1
					delete!(parser.active_rules, rule_id)
				else
					pop!(rulestack)
					parser.active_rules[rule_id] = rulestack
				end
			end
		end
		if parser.debug
			@printf("Reducing %d symbol(s) using rule %d (handle %d).\\n",
				num_rhs, rule_id, handle)
		end
	""")
	else
	pr("""
		if parser.debug
			@printf("Reducing %d symbol(s) using rule %d.\\n", num_rhs, rule_id)
		end
	""")
	end
	pr("""
		parser.dist_to_jump = num_rhs
		args = parser.symbols[end - num_rhs + 1 : end]
		deleteat!(parser.symbols, length(parser.symbols) - num_rhs + 1 : length(parser.symbols))
		if rule_id in keys(parser.semantics)
			rule_ret = parser.semantics[rule_id](args, true, rule_ret)
		end
		push!(parser.symbols, Dict{String, Any}( "is_term" => false, "id" => lhs_id, "val" => rule_ret ))
	end
	""")

	# parse function
	call_start_func = @sprintf("\tstate_%d(parser)", start_idx)

	pr("""
	function parse(parser::Parser)
		reset(parser)
		get_next_lookahead(parser)""")
	pr(call_start_func)
	pr("""
		if length(parser.symbols) < 1 || !parser.accepted
			return nothing
		end
		return parser.symbols[end]
	end""")
end


function write_closure(tables::Dict{String, Any}, state_idx::t_idx,
	outfile::IOStream, gen_partials::Bool = false)
	# lalr(1) tables
	shift_tab = tables["shift"]["elems"][state_idx + 1]
	reduce_tab = tables["reduce"]["elems"][state_idx + 1]
	jump_tab = tables["jump"]["elems"][state_idx + 1]

	# index helper tables
	termidx_tab = tables["indices"]["term_idx"]
	nontermidx_tab = tables["indices"]["nonterm_idx"]
	semanticidx_tab = tables["indices"]["semantic_idx"]
	numrhs_tab = tables["indices"]["num_rhs_syms"]
	lhsidx_tab = tables["indices"]["lhs_idx"]

	# partial rule tables
	part_term = tables["partials_rule_term"]["elems"][state_idx + 1]
	part_nonterm = tables["partials_rule_nonterm"]["elems"][state_idx + 1]
	part_term_len = tables["partials_matchlen_term"]["elems"][state_idx + 1]
	part_nonterm_len = tables["partials_matchlen_nonterm"]["elems"][state_idx + 1]
	part_nonterm_lhs = tables["partials_lhs_nonterm"]["elems"][state_idx + 1]

	# special values
	acc_token :: t_idx = tables["consts"]["acc"]
	err_token :: t_idx = tables["consts"]["err"]
	end_token :: t_idx = tables["consts"]["end"]

	num_terms = length(shift_tab)
	num_nonterms = length(jump_tab)

	# shortcut for writing to the file
	function pr(str :: AbstractString)
		write(outfile, str * "\n")
	end

	has_shift_entry = has_table_entry(shift_tab, err_token)
	has_jump_entry = has_table_entry(jump_tab, err_token)

	state_func = @sprintf("state_%d", state_idx)
	pr("\nfunction " * state_func * "(parser::Parser)")

	if has_shift_entry
		pr("\tnext_state::Union{Function, Nothing} = nothing")
	end

	pr("\tla = t_id(parser.lookahead[\"id\"])")
	num_la_cases = 0

	pr("\tif parser.debug")
	pr("\t\t@printf(\"Entering state " * string(state_idx) *
		", lookahead: %d.\\n\", la)")
	pr("\tend")

	rules_term_id = Dict()
	acc_term_id = []
	for term_idx = 0 : num_terms - 1
		term_id = get_table_id(termidx_tab, term_idx)

		newstate_idx = shift_tab[term_idx + 1]
		rule_idx = reduce_tab[term_idx + 1]

		# shift
		if newstate_idx != err_token
			term_strid = get_table_strid(termidx_tab, term_idx)
			term_id_str = id_to_str(term_id, end_token)

			ifstr = num_la_cases > 0 ? "elseif" : "if"
			pr("\t" * ifstr * " la == " * term_id_str *
				" # id: " * term_strid *
				", idx: " * string(term_idx))
			pr("\t\tnext_state = state_" * string(newstate_idx))

			num_la_cases += 1

			if gen_partials
				partial_idx = part_term[term_idx + 1]
				if partial_idx != err_token
					partial_id = get_table_id(semanticidx_tab, partial_idx)
					partial_len = part_term_len[term_idx + 1]
					pr("\t\tif parser.use_partials")
					pr("\t\t\tapply_partial_rule(parser, " * string(partial_id) * ", " * string(partial_len) * ", true)")
					pr("\t\tend")
				end
			end

		elseif rule_idx != err_token
			# accept
			if rule_idx == acc_token
				push!(acc_term_id, term_id)
			# reduce
			else
				if !(rule_idx in keys(rules_term_id))
					rules_term_id[rule_idx] = []
				end
				push!(rules_term_id[rule_idx], term_id)
			end
		end
	end

	# reduce
	for (rule_idx, term_id) in rules_term_id
		ifstr = num_la_cases > 0 ? "elseif " : "if "

		idstrs = []
		idxstrs = []
		for id in term_id
			idstr = "la == " * id_to_str(id, end_token)
			idxstr = string(get_table_index(termidx_tab, id))

			push!(idstrs, idstr)
			push!(idxstrs, idxstr)
		end
		termids = join(idstrs, " || ")
		termidx = join(idxstrs, ", ")

		pr("\t" * ifstr * termids * " # idx: " * termidx)

		num_rhs = numrhs_tab[rule_idx + 1]
		lhs_idx = lhsidx_tab[rule_idx + 1]
		lhs_id = get_table_id(nontermidx_tab, lhs_idx)
		rule_id = get_table_id(semanticidx_tab, rule_idx)
		pr("\t\tapply_rule(parser, " * string(rule_id) * ", " *
			string(num_rhs) * ", " * string(lhs_id) * ")")

		num_la_cases += 1
	end

	if length(acc_term_id) > 0
		ifstr = num_la_cases > 0 ? "elseif " : "if "

		idstrs = []
		idxstrs = []
		for id in acc_term_id
			idstr = "la == " * id_to_str(id, end_token)
			idxstr = string(get_table_index(termidx_tab, id))

			push!(idstrs, idstr)
			push!(idxstrs, idxstr)
		end
		termids = join(idstrs, " || ")
		termidx = join(idxstrs, ", ")

		pr("\t" * ifstr * termids * " # idx: " * termidx)
		pr("\t\tparser.accepted = true")

		num_la_cases += 1
	end

	if num_la_cases > 0
		pr("\telse")
		pr("\t\tthrow(SystemError(\"Invalid terminal transition from state " *
			string(state_idx) *
			". Lookahead id: \" * string(la) * \"" *
			".\"))")
		pr("\tend")  # if(la == ...)
	end

	# shift
	if has_shift_entry
		pr("\tif next_state !== nothing")
		pr("\t\tpush_lookahead(parser)")
		pr("\t\tnext_state(parser)")
		pr("\tend")
	end

	# jump
	if has_jump_entry
		pr("\twhile parser.dist_to_jump == 0 && length(parser.symbols) > 0 && !parser.accepted")
		pr("\t\ttopsym = parser.symbols[end]")
		pr("\t\tif topsym[\"is_term\"]")
		pr("\t\t\tbreak")
		pr("\t\tend")

		num_top_cases = 0
		pr("\t\ttop = topsym[\"id\"]")
		for nonterm_idx = 0 : num_nonterms - 1
			nonterm_id = get_table_id(nontermidx_tab, nonterm_idx)
			jump_state_idx = jump_tab[nonterm_idx + 1]

			if jump_state_idx != err_token
				nonterm_strid = get_table_strid(nontermidx_tab, nonterm_idx)

				ifstr = num_top_cases > 0 ? "elseif" : "if"
				pr("\t\t" * ifstr * " top == " * string(nonterm_id) *
					" # id: " * string(nonterm_strid) *
					", idx: " * string(nonterm_idx))

				num_top_cases += 1

				if gen_partials
					lhs_id = part_nonterm_lhs[nonterm_idx + 1]
					if lhs_id != err_token
						lhs_idx = get_table_index(nontermidx_tab, lhs_id)
						partial_idx = part_nonterm[lhs_idx]
						if partial_idx != err_token
							partial_id = get_table_id(semanticidx_tab, partial_idx)
							partial_len = part_nonterm_len[lhs_idx]
							pr("\t\t\tif parser.use_partials")
							pr("\t\t\t\tapply_partial_rule(parser, " *
								string(partial_id) * ", " *
								string(partial_len) * ", false)")
							pr("\t\t\tend")
						end
					end
				end

				pr("\t\t\tstate_" * string(jump_state_idx) * "(parser)")
			end
		end

		if num_top_cases > 0
			pr("\t\telse")
			pr("\t\t\tthrow(SystemError(\"Invalid nonterminal transition from state " *
				string(state_idx) *
				". Top symbol id: \" * string(top) * \"" *
				".\"))")
			pr("\t\tend")
		end

		pr("\tend")  # while
	end

	pr("\tparser.dist_to_jump -= 1")

	pr("\tif parser.debug")
	pr("\t\t@printf(\"Exiting state " * string(state_idx) *
		", lookahead: %d, distance to jump: %s.\\n\", la, parser.dist_to_jump)")
	pr("\tend")

	pr("end")  # state function
end


function create_parser(tables::Dict{String, Any}, parser_name::AbstractString,
	outfile_name::AbstractString, gen_partials::Bool = false)
	num_states = length(tables["shift"]["elems"])
	if num_states == 0
		printstyled(stderr, "Error: No states defined.\n", color=:red, bold=true)
		return false
	end

	outfile = open(outfile_name, write = true)

	write(outfile,
		"#\n# Parser created using liblalr1 by Tobias Weber, 2020-2024.\n" *
		"# DOI: https://doi.org/10.5281/zenodo.6987396\n#\n\n")

	write(outfile, "module " * parser_name * "\n\n")

	# basic parser
	write_parser(tables, outfile, gen_partials)

	# closures
	for state_idx = 0 : num_states - 1
		write_closure(tables, state_idx, outfile, gen_partials)
	end

	write(outfile, "\nend\n")
	close(outfile)

	return true
end

end  # module parsergen


function parsergen_main(args)
	if length(args) < 1
		printstyled(stderr, "Please give a toml parsing table file.\n",
			color=:red, bold=true)
		return -1
	end

	tablesfile_name = args[1]
	parser_name = splitext(tablesfile_name)[1] * "_parser"
	outfile_name = parser_name * ".jl"

	@printf("Creating parser \"%s\" -> \"%s\".\n", tablesfile_name, outfile_name)

	tables = TOML.parsefile(tablesfile_name)
	printstyled(tables["infos"] * "\n", color=:blue, bold=true)

	gen_partials = true
	elapsed_time = (@elapsed parser_ok =
		parsergen.create_parser(tables, parser_name, outfile_name, gen_partials))
	if !parser_ok
		printstyled(stderr, "Error: Failed creating parser.\n", color=:red, bold=true)
		return -1
	end

	elapsed_str = @sprintf("Parser created in %.4g s.\n", elapsed_time)
	println(elapsed_str)

	return 0
end


@static if VERSION >= v"1.11"
	function (@main)(args)
		return parsergen_main(args)
	end
@static else
	exit(parsergen_main(ARGS))
end
