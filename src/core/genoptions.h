/**
 * code/table generator options
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 5-nov-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_GEN_OPTIONS_H__
#define __LALR1_GEN_OPTIONS_H__

#include "types.h"


namespace lalr1 {

class GenOptions
{
public:
	void SetUseOpChar(bool b = true);
	void SetGenDebugCode(bool b = true);
	void SetGenErrorCode(bool b = true);
	void SetGenPartialMatches(bool b = true);
	void SetAcceptingRule(t_semantic_id rule_id);
	void SetStartingState(t_index start);

	bool GetUseOpChar() const;
	bool GetGenDebugCode() const;
	bool GetGenErrorCode() const;
	bool GetGenPartialMatches() const;
	t_semantic_id GetAcceptingRule() const;
	t_index GetStartingState() const;

	bool GetUseStateNames() const;
	void SetUseStateNames(bool b = true);

	bool GetUseNegativeTableValues() const;
	void SetUseNegativeTableValues(bool b = true);


private:
	bool m_useOpChar{true};                     // use printable character for operators if possible
	bool m_genDebugCode{true};                  // generate debug code in parser output
	bool m_genErrorCode{true};                  // generate error handling code in parser output
	bool m_genPartialMatches{true};             // generates code for handling partial rule matches
	bool m_useStateNames{false};                // name closure functions
	bool m_useNegativeTableValues{true};

	t_semantic_id m_accepting_rule{0};          // rule which leads to accepting the grammar
	t_index m_starting_state{0};                // parser starting state
};

} // namespace lalr1

#endif
