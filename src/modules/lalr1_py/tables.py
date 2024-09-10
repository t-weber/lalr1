#
# parser table helpers
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date 2-oct-2022
# @license see 'LICENSE' file
#
# References:
#	- https://doi.org/10.1016/0020-0190(88)90061-0
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

	raise IndexError("No id for table index {0}.".format(id))
	return None


#
# get the token or terminal string id of an internal table index
#
def get_table_strid(idx_tab, idx):
	for entry in idx_tab:
		if entry[1] == idx:
			return entry[2]

	raise IndexError("No string id for table index {0}.".format(idx))
	return None


def has_table_entry(row, err_token):
	for elem in row:
		if elem != err_token:
			return True
	return False


def id_to_str(id, end_token):
	if id == end_token and id >= 0:
		return f"0x{id:x}"
		#return "end_id"
	if isinstance(id, str):
		return "\"" + str(id) + "\""
	return str(id)
