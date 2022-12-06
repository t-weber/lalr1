/**
 * generates an lalr(1) recursive ascent parser from a collection
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date aug-2022
 * @license see 'LICENSE' file
 *
 * Principal reference:
 *	- https://doi.org/10.1016/0020-0190(88)90061-0
 * Further references:
 *	- "Compilerbau Teil 1", ISBN: 3-486-25294-1 (1999)
 *	- "Übersetzerbau", ISBN: 978-3540653899 (1999, 2013)
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/lr1.html
 *	- https://www.cs.ecu.edu/karl/5220/spr16/Notes/Bottom-up/slr1table.html
 *	- https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf
 *	- https://en.wikipedia.org/wiki/LR_parser
 */

#ifndef __LALR1_PARSERGEN_H__
#define __LALR1_PARSERGEN_H__

#include "collection.h"
#include "genoptions.h"

namespace lalr1 {

class ParserGen;
using ParserGenPtr = std::shared_ptr<ParserGen>;


class ParserGen : public GenOptions
{
public:
	ParserGen(const CollectionPtr& coll);
	ParserGen() = delete;
	~ParserGen() = default;

	bool SaveParser(const std::string& file, const std::string& class_name = "ParserRecAsc") const;
	bool GetStopOnConflicts() const;


private:
	const CollectionPtr m_collection{};
};

} // namespace lalr1

#endif
