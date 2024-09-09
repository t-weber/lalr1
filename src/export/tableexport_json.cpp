/**
 * exports lalr(1) tables to json
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

#include "tableexport_json.h"
#include "options.h"
#include "timer.h"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>


namespace lalr1 {


/**
 * save a parsing tables to json
 * @see https://en.wikipedia.org/wiki/JSON
 */
void TableExportJSON::SaveParseTable(const t_table& tab,
	std::ostream& ostr, const std::string& var,
	const std::string& row_label, const std::string& col_label,
	const std::string& elem_label,
	const std::unordered_map<typename t_table::value_type, int>* value_map)
{
	ostr << "\"" << var << "\" : {\n";
	ostr << "\t\"rows\" : " << tab.size1() << ",\n";
	ostr << "\t\"cols\" : " << tab.size2() << ",\n";
	ostr << "\t\"row_label\" : \"" << row_label << "\",\n";
	ostr << "\t\"col_label\" : \"" << col_label << "\",\n";
	ostr << "\t\"elem_label\" : \"" << elem_label << "\",\n";

	ostr << "\t\"elems\" : [\n";
	for(std::size_t row = 0; row < tab.size1(); ++row)
	{
		ostr << "\t\t[ ";
		for(std::size_t col = 0; col < tab.size2(); ++col)
		{
			const typename t_table::value_type& entry = tab(row, col);
			/*if(entry == tab.GetErrorVal() || entry == tab.GetAcceptVal())
				ostr << "0x" << std::hex << entry << std::dec;
			else
				ostr << entry;*/

			bool has_value = false;
			if(value_map)
			{
				if(auto iter_val = value_map->find(entry); iter_val != value_map->end())
				{
					// write mapped value
					ostr << iter_val->second;
					has_value = true;
				}
			}

			// write unmapped value otherwise
			if(!has_value)
				ostr << entry;

			if(col < tab.size2() - 1)
				ostr << ",";
			ostr << " ";
		}
		ostr << "]";
		if(row < tab.size1() - 1)
			ostr << ",";
		ostr << "\n";
	}
	ostr << "\t]\n";

	ostr << "}";
}



/**
 * save the parsing tables to json
 * @see https://en.wikipedia.org/wiki/JSON
 */
bool TableExportJSON::SaveParseTables(const TableGen& tab, const std::string& file)
{
	std::unordered_map<t_index, int> special_values
	{
		{ ERROR_VAL, -1 },
		{ ACCEPT_VAL, -2 },
	};
	std::unordered_map<t_symbol_id, int> special_idents
	{
		{ END_IDENT, -1 },
		{ EPS_IDENT, -2 },
	};

	std::ofstream ofstr{file};
	if(!ofstr)
		return false;

	ofstr << "{\n";

	// meta infos
	ofstr << "\"infos\" : ";
	ofstr << "\"Parsing tables created on " << get_timestamp();
	ofstr << " using liblalr1 by Tobias Weber, 2020-2024";
	ofstr << " (DOI: https://doi.org/10.5281/zenodo.6987396).\",\n";

	// constants
	ofstr << "\n\"consts\" : {\n";
	//ofstr << "\t\"acc_rule\" : " << m_accepting_rule << ",\n";

	if(tab.GetUseNegativeTableValues())
	{
		ofstr << "\t\"err\" : " << special_values[ERROR_VAL] << ",\n";
		ofstr << "\t\"acc\" : " << special_values[ACCEPT_VAL] << ",\n";
		ofstr << "\t\"eps\" : " << special_idents[EPS_IDENT] << ",\n";
		ofstr << "\t\"end\" : " << special_idents[END_IDENT] << ",\n";
	}
	else
	{
		/*ofstr << "\t\"err\" : 0x" << std::hex << ERROR_VAL << std::dec << ",\n";
		ofstr << "\t\"acc\" : 0x" << std::hex << ACCEPT_VAL << std::dec << ",\n";
		ofstr << "\t\"eps\" : 0x" << std::hex << EPS_IDENT << std::dec << ",\n";
		ofstr << "\t\"end\" : 0x" << std::hex << END_IDENT << std::dec << "\n";*/
		ofstr << "\t\"err\" : " << ERROR_VAL << ",\n";
		ofstr << "\t\"acc\" : " << ACCEPT_VAL << ",\n";
		ofstr << "\t\"eps\" : " << EPS_IDENT << ",\n";
		ofstr << "\t\"end\" : " << END_IDENT << ",\n";
	}

	t_index acc_rule_idx = tab.GetTableIndex(tab.GetAcceptingRule(), IndexTableKind::SEMANTIC);
	ofstr << "\t\"accept\" : " << acc_rule_idx << ",\n";
	ofstr << "\t\"start\" : " << tab.GetStartingState() << "\n";

	ofstr << "},\n\n";

	const std::unordered_map<t_index, int>* spec_vals = &special_values;
	if(!tab.GetUseNegativeTableValues())
		spec_vals = nullptr;

	// lalr(1) tables
	SaveParseTable(tab.GetShiftTable(), ofstr,
		"shift", "state", "terminal", "state", spec_vals);
	ofstr << ",\n\n";
	SaveParseTable(tab.GetReduceTable(), ofstr,
		"reduce", "state", "lookahead", "rule index", spec_vals);
	ofstr << ",\n\n";
	SaveParseTable(tab.GetJumpTable(), ofstr,
		"jump", "state", "nonterminal", "state", spec_vals);
	ofstr << ",\n\n";

	// partial match tables
	if(tab.GetGenPartialMatches())
	{
		SaveParseTable(tab.GetPartialsRuleTerm(), ofstr,
			"partials_rule_term", "state", "terminal", "rule index", spec_vals);
		ofstr << ",\n\n";
		SaveParseTable(tab.GetPartialsMatchLengthTerm(), ofstr,
			"partials_matchlen_term", "state", "terminal", "length");
		ofstr << ",\n\n";
		SaveParseTable(tab.GetPartialsRuleNonterm(), ofstr,
			"partials_rule_nonterm", "state", "nonterminal", "rule index", spec_vals);
		ofstr << ",\n\n";
		SaveParseTable(tab.GetPartialsMatchLengthNonterm(), ofstr,
			"partials_matchlen_nonterm", "state", "nonterminal", "length");
		ofstr << ",\n\n";
		SaveParseTable(tab.GetPartialsNontermLhsId(), ofstr,
			"partials_lhs_nonterm", "state", "nonterminal", "lhs nonterminal id", spec_vals);
		ofstr << ",\n";
	}

	// terminal symbol indices
	const t_mapIdIdx& mapTermIdx = tab.GetTermIndexMap();
	const t_mapIdStrId& mapTermStrIds = tab.GetTermStringIdMap();
	ofstr << "\n\"term_idx\" : [\n";
	for(auto iter = mapTermIdx.begin(); iter != mapTermIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;

		ofstr << "\t[ ";
		if(auto iter_special = special_idents.find(id);
			tab.GetUseNegativeTableValues() && iter_special != special_idents.end())
		{
			ofstr << iter_special->second;
		}
		else
		{
			if(tab.GetUseOpChar() && isprintable(id))
				ofstr << "\"" << get_escaped_char(char(id)) << "\"";
			else
				ofstr << id;
		}
		ofstr << ", " << idx;

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id); iterStrId != mapTermStrIds.end())
			ofstr << ", \"" << iterStrId->second << "\"";

		ofstr << " ]";

		if(std::next(iter, 1) != mapTermIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "],\n";

	// non-terminal symbol indices
	const t_mapIdIdx& mapNonTermIdx = tab.GetNontermIndexMap();
	const t_mapIdStrId& mapNonTermStrIds = tab.GetNontermStringIdMap();
	ofstr << "\n\"nonterm_idx\" : [\n";
	for(auto iter = mapNonTermIdx.begin(); iter != mapNonTermIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;
		ofstr << "\t[ " << id << ", " << idx;

		// get string identifier
		if(auto iterStrId = mapNonTermStrIds.find(id); iterStrId != mapNonTermStrIds.end())
			ofstr << ", \"" << iterStrId->second << "\"";

		ofstr << " ]";

		if(std::next(iter, 1) != mapNonTermIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "],\n";

	// terminal operator precedences
	const t_mapIdPrec& mapTermPrec = tab.GetTermPrecMap();
	ofstr << "\n\"term_prec\" : [\n";
	for(auto iter = mapTermPrec.begin(); iter != mapTermPrec.end(); std::advance(iter, 1))
	{
		const auto& [id, prec] = *iter;

		ofstr << "\t[ " << id << ", " << prec << " ]";

		if(std::next(iter, 1) != mapTermPrec.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "],\n";

	// terminal operator associativities
	const t_mapIdAssoc& mapTermAssoc = tab.GetTermAssocMap();

	ofstr << "\n\"term_assoc\" : [\n";
	for(auto iter = mapTermAssoc.begin(); iter != mapTermAssoc.end(); std::advance(iter, 1))
	{
		const auto& [id, assoc] = *iter;

		ofstr << "\t[ " << id << ", \"" << assoc << "\" ]";

		if(std::next(iter, 1) != mapTermAssoc.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "],\n";

	// semantic rule indices
	const t_mapIdIdx& mapSemanticIdx = tab.GetSemanticIndexMap();
	ofstr << "\n\"semantic_idx\" : [\n";
	for(auto iter = mapSemanticIdx.begin(); iter != mapSemanticIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;
		ofstr << "\t[ " << id << ", " << idx << " ]";
		if(std::next(iter, 1) != mapSemanticIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "],\n";

	// number of symbols on right-hand side of rule
	const auto& numRhsSymsPerRule = tab.GetNumRhsSymbolsPerRule();
	ofstr << "\n\"num_rhs_syms\" : [ ";
	for(auto iter = numRhsSymsPerRule.begin(); iter != numRhsSymsPerRule.end(); std::advance(iter, 1))
	{
		ofstr << *iter;
		if(std::next(iter, 1) != numRhsSymsPerRule.end())
			ofstr << ",";
		ofstr << " ";
	}
	ofstr << "],\n";

	// index of lhs nonterminal in rule
	const auto& ruleLhsIdx = tab.GetRuleLhsIndices();
	ofstr << "\n\"lhs_idx\" : [ ";
	for(auto iter = ruleLhsIdx.begin(); iter != ruleLhsIdx.end(); std::advance(iter, 1))
	{
		ofstr << *iter;
		if(std::next(iter, 1) != ruleLhsIdx.end())
			ofstr << ",";
		ofstr << " ";
	}
	ofstr << "]\n";

	ofstr << "}" << std::endl;
	return true;
}

} // namespace lalr1
