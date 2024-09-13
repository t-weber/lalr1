#
# helper functions for lalr(1) parser tables
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 8-sep-2024
# @license see 'LICENSE' file
#
# References:
#	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
#	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
#

module tables


#
# table index and id types
#
const t_idx = Integer
const t_id  = Integer


#
# get the internal table index of a token or nonterminal id
#
function get_table_index(idx_tab, id) :: t_idx
	for entry in idx_tab
		if entry[1] == t_id(id)
			return entry[2] + 1
		end
	end

	throw(DomainError(id, "No table index for this id."))
	return 0
end


#
# get the token or terminal id of an internal table index
#
function get_table_id(idx_tab, idx :: t_idx) :: t_id
	for entry in idx_tab
		if entry[2] == idx
			return t_id(entry[1])
		end
	end

	throw(DomainError(idx, "No id for this table index."))
	return 0
end


#
# get the token or terminal string id of an internal table index
#
function get_table_strid(idx_tab, idx :: t_idx) :: String
	for entry in idx_tab
		if entry[2] == idx
			return entry[3]
		end
	end

	throw(DomainError(idx, "No string id for table index."))
	return 0
end


function has_table_entry(row, err_token) :: Bool
	for elem in row
		if elem != err_token
			return true
		end
	end
	return false
end


function id_to_str(id, end_token) :: String
	if id == end_token && id >= 0
		return "0x" * string(id, base = 16)
		#return "end_id"
	elseif typeof(id) == String
		return "\"" * id * "\""
	elseif typeof(id) == Char
		return "'" * id * "'"
	end

	return string(id)
end


end
