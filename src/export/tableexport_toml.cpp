/**
 * exports lalr(1) tables to toml
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 08-sep-2024
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

#include "tableexport_toml.h"
#include "options.h"
#include "timer.h"

#include <fstream>
#include <unordered_map>


namespace lalr1 {


/**
 * save a parsing tables to toml
 * @see https://en.wikipedia.org/wiki/TOML
 */
void TableExportTOML::SaveParseTable(const t_table& tab,
	std::ostream& ostr, const std::string& var,
	const std::string& row_label, const std::string& col_label,
	const std::string& elem_label,
	const std::unordered_map<typename t_table::value_type, int>* value_map)
{
	ostr << "[" << var << "]\n";
	ostr << "\trows = " << tab.size1() << "\n";
	ostr << "\tcols = " << tab.size2() << "\n";
	ostr << "\trow_label = \"" << row_label << "\"\n";
	ostr << "\tcol_label = \"" << col_label << "\"\n";
	ostr << "\telem_label = \"" << elem_label << "\"\n";

	ostr << "\telems = [ # " << elem_label << "\n";
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

		if(row_label != "")
			ostr << " # " << row_label << " " << row;

		ostr << "\n";
	}
	ostr << "\t]\n";
}



/**
 * save the parsing tables to toml
 * @see https://en.wikipedia.org/wiki/TOML
 */
bool TableExportTOML::SaveParseTables(const TableGen& tab, const std::string& file)
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

	// --------------------------------------------------------------------
	// meta infos
	ofstr << "\"infos\" = ";
	ofstr << "\"Parsing tables created on " << get_timestamp();
	ofstr << " using liblalr1 by Tobias Weber, 2020-2024";
	ofstr << " (DOI: https://doi.org/10.5281/zenodo.6987396).\"\n";
	// --------------------------------------------------------------------

	// --------------------------------------------------------------------
	// constants
	ofstr << "\n[consts]\n";
	//ofstr << "\tacc_rule = " << m_accepting_rule << "\n";

	if(tab.GetUseNegativeTableValues())
	{
		ofstr << "\terr = " << special_values[ERROR_VAL] << "\n";
		ofstr << "\tacc = " << special_values[ACCEPT_VAL] << "\n";
		ofstr << "\teps = " << special_idents[EPS_IDENT] << "\n";
		ofstr << "\tend = " << special_idents[END_IDENT] << "\n";
	}
	else
	{
		/*ofstr << "\terr = 0x" << std::hex << ERROR_VAL << std::dec << "\n";
		ofstr << "\tacc = 0x" << std::hex << ACCEPT_VAL << std::dec << "\n";
		ofstr << "\teps = 0x" << std::hex << EPS_IDENT << std::dec << "\n";
		ofstr << "\tend = 0x" << std::hex << END_IDENT << std::dec << "\n";*/
		ofstr << "\terr = " << ERROR_VAL << "\n";
		ofstr << "\tacc = " << ACCEPT_VAL << "\n";
		ofstr << "\teps = " << EPS_IDENT << "\n";
		ofstr << "\tend = " << END_IDENT << "\n";
	}

	t_index acc_rule_idx = tab.GetTableIndex(tab.GetAcceptingRule(), IndexTableKind::SEMANTIC);
	ofstr << "\taccept = " << acc_rule_idx << "\n";
	ofstr << "\tstart = " << tab.GetStartingState() << "\n";

	ofstr << "\n\n";
	// --------------------------------------------------------------------

	// --------------------------------------------------------------------
	const std::unordered_map<t_index, int>* spec_vals = &special_values;
	if(!tab.GetUseNegativeTableValues())
		spec_vals = nullptr;

	// lalr(1) tables
	SaveParseTable(tab.GetShiftTable(), ofstr,
		"shift", "state", "terminal", "state", spec_vals);
	ofstr << "\n\n";
	SaveParseTable(tab.GetReduceTable(), ofstr,
		"reduce", "state", "lookahead", "rule index", spec_vals);
	ofstr << "\n\n";
	SaveParseTable(tab.GetJumpTable(), ofstr,
		"jump", "state", "nonterminal", "state", spec_vals);
	ofstr << "\n\n";
	// --------------------------------------------------------------------

	// --------------------------------------------------------------------
	// partial match tables
	if(tab.GetGenPartialMatches())
	{
		SaveParseTable(tab.GetPartialsRuleTerm(), ofstr,
			"partials_rule_term", "state", "terminal", "rule index", spec_vals);
		ofstr << "\n\n";
		SaveParseTable(tab.GetPartialsMatchLengthTerm(), ofstr,
			"partials_matchlen_term", "state", "terminal", "length");
		ofstr << "\n\n";
		SaveParseTable(tab.GetPartialsRuleNonterm(), ofstr,
			"partials_rule_nonterm", "state", "nonterminal", "rule index", spec_vals);
		ofstr << "\n\n";
		SaveParseTable(tab.GetPartialsMatchLengthNonterm(), ofstr,
			"partials_matchlen_nonterm", "state", "nonterminal", "length");
		ofstr << "\n\n";
		SaveParseTable(tab.GetPartialsNontermLhsId(), ofstr,
			"partials_lhs_nonterm", "state", "nonterminal", "lhs nonterminal id", spec_vals);
		ofstr << "\n";
	}
	// --------------------------------------------------------------------

	// --------------------------------------------------------------------
	ofstr << "\n[precedences]\n";

	// terminal operator precedences
	const t_mapIdPrec& mapTermPrec = tab.GetTermPrecMap();
	const t_mapIdStrId& mapTermStrIds = tab.GetTermStringIdMap();

	ofstr << "\tterm_prec = [ # [ term id, prec ] \n";
	for(auto iter = mapTermPrec.begin(); iter != mapTermPrec.end(); std::advance(iter, 1))
	{
		const auto& [id, prec] = *iter;

		ofstr << "\t\t[ " << id << ", " << prec << " ]";

		if(std::next(iter, 1) != mapTermPrec.end())
			ofstr << ",";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id); iterStrId != mapTermStrIds.end())
			ofstr << " # " << iterStrId->second;

		ofstr << "\n";
	}
	ofstr << "\t]\n";

	// terminal operator associativities
	const t_mapIdAssoc& mapTermAssoc = tab.GetTermAssocMap();

	ofstr << "\n\tterm_assoc = [ # [ term id, assoc ] \n";
	for(auto iter = mapTermAssoc.begin(); iter != mapTermAssoc.end(); std::advance(iter, 1))
	{
		const auto& [id, assoc] = *iter;

		ofstr << "\t\t[ " << id << ", \"" << assoc << "\" ]";

		if(std::next(iter, 1) != mapTermAssoc.end())
			ofstr << ",";

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id); iterStrId != mapTermStrIds.end())
			ofstr << " # " << iterStrId->second;

		ofstr << "\n";
	}
	ofstr << "\t]\n\n";
	// --------------------------------------------------------------------

	// --------------------------------------------------------------------
	ofstr << "\n[indices]\n";
	// terminal symbol indices
	const t_mapIdIdx& mapTermIdx = tab.GetTermIndexMap();
	ofstr << "\tterm_idx = [ # [ term id, term idx, term str_id ]\n";
	for(auto iter = mapTermIdx.begin(); iter != mapTermIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;

		ofstr << "\t\t[ ";
		if(auto iter_special = special_idents.find(id);
			tab.GetUseNegativeTableValues() && iter_special != special_idents.end())
		{
			ofstr << iter_special->second;
		}
		else
		{
			//if(tab.GetUseOpChar() && isprintable(id))
			//	ofstr << "\"" << get_escaped_char(char(id)) << "\"";
			//else
				ofstr << id;
		}
		ofstr << ", " << idx;

		// get string identifier
		if(auto iterStrId = mapTermStrIds.find(id); iterStrId != mapTermStrIds.end())
			ofstr << ", \"" << iterStrId->second << "\"";

		ofstr << " ]";

		if(std::next(iter, 1) != mapTermIdx.end())
			ofstr << ",";
		//if(isprintable(id))
		//	ofstr << " # " << char(id);
		ofstr << "\n";
	}
	ofstr << "\t]\n";

	// non-terminal symbol indices
	const t_mapIdIdx& mapNonTermIdx = tab.GetNontermIndexMap();
	const t_mapIdStrId& mapNonTermStrIds = tab.GetNontermStringIdMap();
	ofstr << "\n\tnonterm_idx = [ # [ nonterm id, nonterm idx, nonterm str_id ] \n";
	for(auto iter = mapNonTermIdx.begin(); iter != mapNonTermIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;
		ofstr << "\t\t[ " << id << ", " << idx;

		// get string identifier
		if(auto iterStrId = mapNonTermStrIds.find(id); iterStrId != mapNonTermStrIds.end())
			ofstr << ", \"" << iterStrId->second << "\"";

		ofstr << " ]";

		if(std::next(iter, 1) != mapNonTermIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "\t]\n";

	// semantic rule indices
	const t_mapIdIdx& mapSemanticIdx = tab.GetSemanticIndexMap();
	ofstr << "\n\tsemantic_idx = [ # [ rule id, rule idx ]\n";
	for(auto iter = mapSemanticIdx.begin(); iter != mapSemanticIdx.end(); std::advance(iter, 1))
	{
		const auto& [id, idx] = *iter;
		ofstr << "\t\t[ " << id << ", " << idx << " ]";
		if(std::next(iter, 1) != mapSemanticIdx.end())
			ofstr << ",";
		ofstr << "\n";
	}
	ofstr << "\t]\n";

	// number of symbols on right-hand side of rule
	const auto& numRhsSymsPerRule = tab.GetNumRhsSymbolsPerRule();
	ofstr << "\n\tnum_rhs_syms = [ ";
	for(auto iter = numRhsSymsPerRule.begin(); iter != numRhsSymsPerRule.end(); std::advance(iter, 1))
	{
		ofstr << *iter;
		if(std::next(iter, 1) != numRhsSymsPerRule.end())
			ofstr << ",";
		ofstr << " ";
	}
	ofstr << "]\n";

	// index of lhs nonterminal in rule
	const auto& ruleLhsIdx = tab.GetRuleLhsIndices();
	ofstr << "\n\tlhs_idx = [ ";
	for(auto iter = ruleLhsIdx.begin(); iter != ruleLhsIdx.end(); std::advance(iter, 1))
	{
		ofstr << *iter;
		if(std::next(iter, 1) != ruleLhsIdx.end())
			ofstr << ",";
		ofstr << " ";
	}
	ofstr << "]";
	// --------------------------------------------------------------------

	ofstr << std::endl;
	return true;
}

} // namespace lalr1
