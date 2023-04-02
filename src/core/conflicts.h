/**
 * conflict resolution
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 2-apr-2023
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_CONFLICTS_H__
#define __LALR1_CONFLICTS_H__

#include "symbol.h"
#include "types.h"

#include <optional>


namespace lalr1 {

	enum ConflictSolution
	{
		DO_SHIFT,
		DO_REDUCE,
		NOT_FOUND
	};


	extern ConflictSolution solve_shift_reduce_conflict(
		const std::optional<t_precedence>& lookback_prec, const std::optional<t_associativity>& lookback_assoc,
		const std::optional<t_precedence>& lookahead_prec, const std::optional<t_associativity>& lookahead_assoc);

	extern ConflictSolution solve_shift_reduce_conflict(
		const TerminalPtr& lookback, const TerminalPtr& lookahead);

} // namespace lalr1


#endif
