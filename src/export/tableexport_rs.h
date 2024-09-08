/**
 * exports lalr(1) tables to rs code
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 07-jun-2020
 * @license see 'LICENSE' file
 *
 * References:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/lr1.html
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/slr1table.html
 *	- https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 *	- https://en.wikipedia.org/wiki/LR_parser
 *	- https://doi.org/10.1016/0020-0190(88)90061-0
 */

#ifndef __LALR1_TABLEEXPORT_RS_H__
#define __LALR1_TABLEEXPORT_RS_H__

#include "tablegen.h"

#include <optional>


namespace lalr1 {


class TableExportRS
{
public:
	static void SaveParseTable(const t_table& tab,
		std::ostream& ostr, const std::string& var,
		const std::string& row_label = "",
		const std::string& col_label = "",
		const std::string& elem_label = "",
		std::optional<std::string> ty_override = std::nullopt);

	// save the parsing tables to rs code
	static bool SaveParseTables(const TableGen& tab, const std::string& file);
};

} // namespace lalr1


#endif
