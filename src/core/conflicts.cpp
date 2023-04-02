/**
 * conflict resolution
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 2-apr-2023
 * @license see 'LICENSE' file
 */

#include "conflicts.h"


namespace lalr1 {


/**
 * tries to solve conflict using operator precedences and associativities
 */
ConflictSolution solve_shift_reduce_conflict(
	const std::optional<t_precedence>& lookback_prec, const std::optional<t_associativity>& lookback_assoc,
	const std::optional<t_precedence>& lookahead_prec, const std::optional<t_associativity>& lookahead_assoc)
{
	ConflictSolution sol{ConflictSolution::NOT_FOUND};

	// both terminals have a precedence
	if(lookback_prec && lookahead_prec)
	{
		if(*lookback_prec < *lookahead_prec)       // shift
			sol = ConflictSolution::DO_SHIFT;
		else if(*lookback_prec > *lookahead_prec)  // reduce
			sol = ConflictSolution::DO_REDUCE;

		// same precedence -> try associativity next
	}

	if(sol == ConflictSolution::NOT_FOUND)
	{
		// both terminals have an associativity
		if(lookback_assoc && lookahead_assoc && *lookback_assoc == *lookahead_assoc)
		{
			if(*lookback_assoc == 'r')      // shift
				sol = ConflictSolution::DO_SHIFT;
			else if(*lookback_assoc == 'l') // reduce
				sol = ConflictSolution::DO_REDUCE;
		}
	}

	return sol;
}


/**
 * tries to solve conflict using operator precedences and associativities
 */
ConflictSolution solve_shift_reduce_conflict(const TerminalPtr& lookback, const TerminalPtr& lookahead)
{
	auto prec_lhs = lookback->GetPrecedence();
	auto prec_rhs = lookahead->GetPrecedence();

	auto assoc_lhs = lookback->GetAssociativity();
	auto assoc_rhs = lookahead->GetAssociativity();

	return solve_shift_reduce_conflict(prec_lhs, assoc_lhs, prec_rhs, assoc_rhs);
}


} // namespace lalr1
