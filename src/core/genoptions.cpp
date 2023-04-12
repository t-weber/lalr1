/**
 * code/table generator options
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 5-nov-2022
 * @license see 'LICENSE' file
 */

#include "genoptions.h"


namespace lalr1 {


/**
 * use printable operator character if possible
 */
void GenOptions::SetUseOpChar(bool b)
{ m_useOpChar = b; }

bool GenOptions::GetUseOpChar() const
{ return m_useOpChar; }


/**
 * generate debug code in parser output
 */
void GenOptions::SetGenDebugCode(bool b)
{ m_genDebugCode = b; }

bool GenOptions::GetGenDebugCode() const
{ return m_genDebugCode; }


/**
 * generate comment strings in parser output
 */
void GenOptions::SetGenComments(bool b)
{ m_genComments = b; }

bool GenOptions::GetGenComments() const
{ return m_genComments; }


/**
 * generate error handling code in parser output
 */
void GenOptions::SetGenErrorCode(bool b)
{ m_genErrorCode = b; }

bool GenOptions::GetGenErrorCode() const
{ return m_genErrorCode; }


/**
 * generate code for handling partial matches
 */
void GenOptions::SetGenPartialMatches(bool b)
{ m_genPartialMatches = b; }

bool GenOptions::GetGenPartialMatches() const
{ return m_genPartialMatches; }


void GenOptions::SetAcceptingRule(t_semantic_id rule_id)
{ m_accepting_rule = rule_id; }

t_semantic_id GenOptions::GetAcceptingRule() const
{ return m_accepting_rule; }


void GenOptions::SetStartingState(t_index start)
{ m_starting_state = start; }

t_index GenOptions::GetStartingState() const
{ return m_starting_state; }


/**
 * use non-terminal names for closures functions
 */
bool GenOptions::GetUseStateNames() const
{ return m_useStateNames; }

void GenOptions::SetUseStateNames(bool b)
{ m_useStateNames = b; }


bool GenOptions::GetUseNegativeTableValues() const
{ return m_useNegativeTableValues; }

void GenOptions::SetUseNegativeTableValues(bool b)
{ m_useNegativeTableValues = b; }

} // namespace lalr1
