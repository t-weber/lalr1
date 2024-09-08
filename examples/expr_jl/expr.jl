#!/bin/env python3
#
# expression parser
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 8-sep-2024
# @license see 'LICENSE' file
#
# References:
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

push!(LOAD_PATH, ".")
push!(LOAD_PATH, "../../src/modules/lalr1_jl")
push!(LOAD_PATH, "../src/modules/lalr1_jl")

using Random
using lexer, ids


# select parser backend
parsing_code = "expr_parser.jl"
parsing_tables = "expr.toml"

if isfile(parsing_code)
	use_recasc = true
elseif isfile(parsing_tables)
	using parser
	use_recasc = false
else
	println(stderr, "No parsing tables found.")
	exit(-1)
end


#
# symbol table
#
symtab = Dict(
	"pi" => pi,
)


#
# function tables
#
functab_0args = Dict(
	"rand01" => () -> rand(Float64)
)

functab_1arg = Dict(
	"sqrt" => sqrt,
	"sin" => sin,
	"cos" => cos,
	"tan" => tan,
	"asin" => asin,
	"acos" => acos,
	"atan" => atan,
)

functab_2args = Dict(
	"pow" => (x, y) -> x^y,
	"atan2" => atan,
	"log" => (b, x) -> log(x) / log(b)
)


#
# semantic rules, same as in expr_grammar.cpp
#
semantics = Dict(
	ids.sem_start_id => (args, done, retval) -> done ? args[1]["val"] : nothing,
	ids.sem_brackets_id => (args, done, retval) -> done ? args[2]["val"] : nothing,

	# binary arithmetic operations
	ids.sem_add_id => (args, done, retval) -> done ? args[1]["val"] + args[3]["val"] : nothing,
	ids.sem_sub_id => (args, done, retval) -> done ? args[1]["val"] - args[3]["val"] : nothing,
	ids.sem_mul_id => (args, done, retval) -> done ? args[1]["val"] * args[3]["val"] : nothing,
	ids.sem_div_id => (args, done, retval) -> done ? args[1]["val"] / args[3]["val"] : nothing,
	ids.sem_mod_id => (args, done, retval) -> done ? args[1]["val"] % args[3]["val"] : nothing,
	ids.sem_pow_id => (args, done, retval) -> done ? args[1]["val"] ^ args[3]["val"] : nothing,

	# unary arithmetic operations
	ids.sem_uadd_id => (args, done, retval) -> done ? +args[2]["val"] : nothing,
	ids.sem_usub_id => (args, done, retval) -> done ? -args[2]["val"] : nothing,

	# function calls
	ids.sem_call0_id => (args, done, retval) ->
		done ? functab_0args[args[1]["val"]]() : nothing,
	ids.sem_call1_id => (args, done, retval) ->
		done ? functab_1arg[args[1]["val"]](args[3]["val"]) : nothing,
	ids.sem_call2_id => (args, done, retval) ->
		done ? functab_2args[args[1]["val"]](args[3]["val"], args[5]["val"]) : nothing,

	# symbols
	ids.sem_real_id => (args, done, retval) -> done ? args[1]["val"] : nothing,
	ids.sem_int_id => (args, done, retval) -> done ? args[1]["val"] : nothing,
	ids.sem_ident_id => (args, done, retval) -> done ? symtab[args[1]["val"]] : nothing,
)



#
# load tables from a json file and run parser
#
#try
	if !use_recasc
		using TOML
		tables = TOML.parsefile(parsing_tables)
	end

	while true
		print("> ")
		input_str = strip(readline())
		if length(input_str) == 0
			continue
		elseif input_str == "exit" || input_str == "quit"
			break
		end

		if use_recasc
			#theparser = expr_parser.Parser()
		else
			theparser = parser.Parser(tables)
		end

		theparser.debug = false
		theparser.use_partials = false
		theparser.semantics = semantics

		input_tokens = lexer.get_tokens(input_str)
		#println(input_tokens)
		push!(input_tokens, [theparser.end_token])
		theparser.input_tokens = input_tokens

		result = parser.parse(theparser)
		if result != nothing
			println(result["val"])
		else
			println(stderr, "Error while parsing.")
		end
	end
#catch err
#	println(stderr, err)
#end
