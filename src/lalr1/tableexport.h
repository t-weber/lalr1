/**
 * exports lalr(1) tables to various formats
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

#ifndef __LALR1_TABEXPORT_H__
#define __LALR1_TABEXPORT_H__

#include "collection.h"


class TableExporter
{
public:
	TableExporter(const Collection* coll);
	TableExporter() = delete;
	~TableExporter() = default;


	/**
	 * save the parsing tables to C++ code
	 */
	bool SaveParseTablesCXX(const std::string& file) const;


	/**
	 * save the parsing tables to Java code
	 */
	bool SaveParseTablesJava(const std::string& file) const;


	/**
	 * save the parsing tables to json
	 * @see https://en.wikipedia.org/wiki/JSON
	 */
	bool SaveParseTablesJSON(const std::string& file) const;


private:
	const Collection* m_collection { nullptr };
};


#endif
