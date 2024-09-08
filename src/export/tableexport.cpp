/**
 * exports lalr(1) tables to c++ code
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

#include "tableexport.h"
#include "options.h"
#include "timer.h"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>


namespace lalr1 {


/**
 * save a parsing table to C++ code
 */
void TableExport::SaveParseTable(const t_table& tab,
	std::ostream& ostr, const std::string& var,
	const std::string& row_label, const std::string& col_label,
	const std::string& elem_label)
{
	ostr << "const lalr1::t_table " << var << "{";
	ostr << tab.size1();
	if(row_label.size())
		ostr << " /*" << row_label << "*/";
	ostr << ", " << tab.size2();
	if(col_label.size())
		ostr << " /*" << col_label << "*/";
	ostr << ", " << "err, acc, ";
	if(tab.GetFillVal() == tab.GetErrorVal())
		ostr << "err, ";
	else if(tab.GetFillVal() == tab.GetAcceptVal())
		ostr << "acc, ";
	else
		ostr << tab.GetFillVal() << ", ";
	ostr << "\n";

	ostr << "{ /*" << elem_label << "*/\n";
	for(std::size_t row = 0; row < tab.size1(); ++row)
	{
		ostr << "\t";
		for(std::size_t col = 0; col < tab.size2(); ++col)
		{
			const typename t_table::value_type& entry = tab(row, col);

			if(entry == tab.GetErrorVal())
				ostr << "err, ";
			else if(entry == tab.GetAcceptVal())
				ostr << "acc, ";
			else
				ostr << entry << ", ";
		}
		ostr << "// " << row_label << " " << row << "\n";
	}
	ostr << "}";
	ostr << "};\n\n";
}



/**
 * save the parsing tables to C++ code
 */
bool TableExport::SaveParseTables(const TableGen& tab, const std::string& file)
{
	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	ofstr << "/*\n";
	ofstr << " * Parsing tables created on " << get_timestamp();
	ofstr << " using liblalr1 by Tobias Weber, 2020-2024.\n";
	ofstr << " * DOI: https://doi.org/10.5281/zenodo.6987396\n";
	ofstr << " */\n\n";

	ofstr << "#ifndef __LALR1_TABLES__\n";
	ofstr << "#define __LALR1_TABLES__\n\n";

	ofstr << "namespace _lalr1_tables {\n\n";

	// save constants
	ofstr << "const constexpr lalr1::t_index err = 0x" << std::hex << ERROR_VAL << std::dec;
	if constexpr(std::is_unsigned_v<t_index>)
		ofstr << "u";
	ofstr << ";\n";

	ofstr << "const constexpr lalr1::t_index acc = 0x" << std::hex << ACCEPT_VAL << std::dec;
	if constexpr(std::is_unsigned_v<t_index>)
		ofstr << "u";
	ofstr << ";\n";

	ofstr << "const constexpr lalr1::t_symbol_id eps = 0x" << std::hex << EPS_IDENT << std::dec;
	if constexpr(std::is_unsigned_v<t_symbol_id>)
		ofstr << "u";
	ofstr << ";\n";

	ofstr << "const constexpr lalr1::t_symbol_id end = 0x" << std::hex << END_IDENT << std::dec;
	if constexpr(std::is_unsigned_v<t_symbol_id>)
		ofstr << "u";
	ofstr << ";\n";

	ofstr << "const constexpr lalr1::t_index start_idx = " << tab.GetStartingState();
	if constexpr(std::is_unsigned_v<t_index>)
		ofstr << "u";
	ofstr << ";\n";

	t_index acc_rule_idx = tab.GetTableIndex(tab.GetAcceptingRule(), IndexTableKind::SEMANTIC);
	ofstr << "const constexpr lalr1::t_index acc_idx = " << acc_rule_idx;
	ofstr << ";\n";

	ofstr << "\n";

	// save lalr(1) tables
	SaveParseTable(tab.GetShiftTable(), ofstr,
		"tab_action_shift", "state", "terminal", "state");
	SaveParseTable(tab.GetReduceTable(), ofstr,
		"tab_action_reduce", "state", "lookahead", "rule index");
	SaveParseTable(tab.GetJumpTable(), ofstr,
		"tab_jump", "state", "nonterminal", "state");

	// save partial match tables
	if(tab.GetGenPartialMatches())
	{
		SaveParseTable(tab.GetPartialsRuleTerm(), ofstr,
			"tab_partials_rule_term", "state", "terminal", "rule index");
		SaveParseTable(tab.GetPartialsMatchLengthTerm(), ofstr,
			"tab_partials_matchlen_term", "state", "terminal", "length");
		SaveParseTable(tab.GetPartialsRuleNonterm(), ofstr,
			"tab_partials_rule_nonterm", "state", "nonterminal", "rule index");
		SaveParseTable(tab.GetPartialsMatchLengthNonterm(), ofstr,
			"tab_partials_matchlen_nonterm", "state", "nonterminal", "length");
	}

	// terminal symbol indices
	const t_mapIdIdx& mapTermIdx = tab.GetTermIndexMap();
	const t_mapIdStrId& mapTermStrIds = tab.GetTermStringIdMap();
	ofstr << "const lalr1::t_mapIdIdx map_term_idx\n{{\n";
	for(const auto& [id, idx] : mapTermIdx)
	{
		ofstr << "\t{ ";
		if(id == EPS_IDENT)
			ofstr << "eps";
		else if(id == END_IDENT)
			ofstr << "end";
		else if(tab.GetUseOpChar() && isprintable(id))
			ofstr << "'" << get_escaped_char(char(id)) << "'";
		else
			ofstr << id;
		ofstr << ", " << idx << " },";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id);
			iterStrId != mapTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "}};\n\n";

	// non-terminal symbol indices
	const t_mapIdIdx& mapNonTermIdx = tab.GetNontermIndexMap();
	const t_mapIdStrId& mapNonTermStrIds = tab.GetNontermStringIdMap();
	ofstr << "const lalr1::t_mapIdIdx map_nonterm_idx\n{{\n";
	for(const auto& [id, idx] : mapNonTermIdx)
	{
		ofstr << "\t{ " << id << ", " << idx << " },";

		// get string identifier
		if(auto iterStrId = mapNonTermStrIds.find(id);
			iterStrId != mapNonTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "}};\n\n";

	// terminal operator precedences
	const t_mapIdPrec& mapTermPrec = tab.GetTermPrecMap();
	ofstr << "const lalr1::t_mapIdPrec map_term_prec\n{{\n";
	for(const auto& [id, prec] : mapTermPrec)
	{
		ofstr << "\t{ " << id << ", " << prec << " },";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id);
			iterStrId != mapTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "}};\n\n";

	// terminal operator associativities
	const t_mapIdAssoc& mapTermAssoc = tab.GetTermAssocMap();
	ofstr << "const lalr1::t_mapIdAssoc map_term_assoc\n{{\n";
	for(const auto& [id, assoc] : mapTermAssoc)
	{
		ofstr << "\t{ " << id << ", '" << assoc << "' },";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id);
			iterStrId != mapTermStrIds.end())
		{
			ofstr << " // " << iterStrId->second;
		}

		ofstr << "\n";
	}
	ofstr << "}};\n\n";

	// semantic rule indices
	const t_mapIdIdx& mapSemanticIdx = tab.GetSemanticIndexMap();
	ofstr << "const lalr1::t_mapSemanticIdIdx map_semantic_idx\n{{\n";
	for(const auto& [id, idx] : mapSemanticIdx)
		ofstr << "\t{ " << id << ", " << idx << " },\n";
	ofstr << "}};\n\n";

	// number of symbols on right-hand side of rule
	const auto& numRhsSymsPerRule = tab.GetNumRhsSymbolsPerRule();
	ofstr << "const lalr1::t_vecIdx vec_num_rhs_syms{{ ";
	for(const auto& val : numRhsSymsPerRule)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	// index of lhs nonterminal in rule
	const auto& ruleLhsIdx = tab.GetRuleLhsIndices();
	ofstr << "const lalr1::t_vecIdx vec_lhs_idx{{ ";
	for(const auto& val : ruleLhsIdx)
		ofstr << val << ", ";
	ofstr << "}};\n\n";

	ofstr << "}\n\n\n";  // end namespace


	// lalr(1) tables getter
	ofstr << "static\nstd::tuple<const lalr1::t_table*, const lalr1::t_table*, const lalr1::t_table*,\n";
	ofstr << "\tconst lalr1::t_vecIdx*, const lalr1::t_vecIdx*>\n";
	ofstr << "get_lalr1_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t&_lalr1_tables::tab_action_shift, &_lalr1_tables::tab_action_reduce, &_lalr1_tables::tab_jump,\n";
	ofstr << "\t\t&_lalr1_tables::vec_num_rhs_syms, &_lalr1_tables::vec_lhs_idx);\n";
	ofstr << "}\n\n";

	// partial match tables getter
	ofstr << "[[maybe_unused]] static\nstd::tuple<const lalr1::t_table*, const lalr1::t_table*,\n";
	ofstr << "\tconst lalr1::t_table*, const lalr1::t_table*>\n";
	ofstr << "get_lalr1_partials_tables()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	if(tab.GetGenPartialMatches())
	{
		ofstr << "\t\t&_lalr1_tables::tab_partials_rule_term, &_lalr1_tables::tab_partials_matchlen_term,\n";
		ofstr << "\t\t&_lalr1_tables::tab_partials_rule_nonterm, &_lalr1_tables::tab_partials_matchlen_nonterm);\n";
	}
	else
	{
		ofstr << "\t\tnullptr, nullptr,\n";
		ofstr << "\t\tnullptr, nullptr);\n";
	}
	ofstr << "}\n\n";

	// index maps getter
	ofstr << "static\nstd::tuple<const lalr1::t_mapIdIdx*, const lalr1::t_mapIdIdx*, const lalr1::t_mapSemanticIdIdx*>\n";
	ofstr << "get_lalr1_table_indices()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t&_lalr1_tables::map_term_idx, &_lalr1_tables::map_nonterm_idx, &_lalr1_tables::map_semantic_idx);\n";
	ofstr << "}\n\n";

	// constants getter
	ofstr << "static constexpr\nstd::tuple<lalr1::t_index, lalr1::t_index, lalr1::t_symbol_id, lalr1::t_symbol_id, lalr1::t_index, lalr1::t_index>\n";
	ofstr << "get_lalr1_constants()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t_lalr1_tables::err, _lalr1_tables::acc, _lalr1_tables::eps, _lalr1_tables::end, _lalr1_tables::start_idx, _lalr1_tables::acc_idx);\n";
	ofstr << "}\n\n";

	// precedence/associativity getter
	ofstr << "[[maybe_unused]] static\nstd::tuple<const lalr1::t_mapIdPrec*, const lalr1::t_mapIdAssoc*>\n";
	ofstr << "get_lalr1_precedences()\n{\n";
	ofstr << "\treturn std::make_tuple(\n";
	ofstr << "\t\t&_lalr1_tables::map_term_prec, &_lalr1_tables::map_term_assoc);\n";
	ofstr << "}\n\n";

	ofstr << "\n#endif" << std::endl;
	return true;
}

} // namespace lalr1
