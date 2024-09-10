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

using Printf
using TOML


module parsergen

push!(LOAD_PATH, ".")
push!(LOAD_PATH, @__DIR__)


using tables: t_idx, t_id, get_table_index, get_table_id,
	get_table_strid, has_table_entry, id_to_str


function write_parser(tables, outfile)
	# TODO
end


function write_closure(tables, state_idx, outfile)
	# TODO
end


function create_parser(tables, outfile_name)
	num_states = length(tables["shift"]["elems"])
	if num_states == 0
		printstyled(stderr, "Error: No states defined.\n", color=:red, bold=true)
		return false
	end

	outfile = open(outfile_name, write = true)
	# basic parser
	write_parser(tables, outfile)

	# closures
	for state_idx = 0 : num_states - 1
		write_closure(tables, state_idx, outfile)
	end
	close(outfile)

	return true
end

end  # module parsergen


function (@main)(args)
	if length(args) < 1
		printstyled(stderr, "Please give a toml parsing table file.\n", color=:red, bold=true)
		return -1
	end

	tablesfile_name = args[1]
	outfile_name = splitext(tablesfile_name)[1] * "_parser.jl"

	@printf("Creating parser \"%s\" -> \"%s\".\n", tablesfile_name, outfile_name)

	tables = TOML.parsefile(tablesfile_name)
	printstyled(tables["infos"] * "\n", color=:blue, bold=true)

	if !parsergen.create_parser(tables, outfile_name)
		printstyled(stderr, "Error: Failed creating parser.\n", color=:red, bold=true)
		return -1
	end

	return 0
end
