/**
 * global options
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 3-sep-2022
 * @license see 'LICENSE' file
 */

#include "options.h"
#include "symbol.h"  // for string id hacks


/**
 * print using colours
 */
void Options::SetUseColour(bool b)
{
	m_useColour = b;
}


/**
 * only use ascii characters
 */
void Options::SetUseAsciiChars(bool b)
{
	m_useAsciiChars = b;

	// hack to set the epsilon and end symbol string identifiers
	g_eps->SetStrId(GetEpsilonChar());
	g_end->SetStrId(GetEndChar());
}


bool Options::GetUseColour() const
{
	return m_useColour;
}


const std::string& Options::GetShiftColour() const
{
	return m_shift_col;
}


const std::string& Options::GetReduceColour() const
{
	return m_reduce_col;
}


const std::string& Options::GetJumpColour() const
{
	return m_jump_col;
}


const std::string& Options::GetTermShiftColour() const
{
	return m_term_shift_col;
}


const std::string& Options::GetTermReduceColour() const
{
	return m_term_reduce_col;
}


const std::string& Options::GetTermJumpColour() const
{
	return m_term_jump_col;
}


const std::string& Options::GetTermNoColour() const
{
	return m_term_no_col;
}


const std::string& Options::GetTermBoldColour() const
{
	return m_term_bold_col;
}


const std::string& Options::GetCursorChar() const
{
	return m_useAsciiChars ? m_cursor_asc : m_cursor;
}


const std::string& Options::GetArrowChar() const
{
	return m_useAsciiChars ? m_arrow_asc : m_arrow;
}


const std::string& Options::GetSeparatorChar() const
{
	return m_useAsciiChars ? m_sep_asc : m_sep;
}


const std::string& Options::GetEpsilonChar() const
{
	return m_useAsciiChars ? m_eps_asc : m_eps;
}

const std::string& Options::GetEndChar() const
{
	return m_useAsciiChars ? m_end_asc : m_end;
}


Options g_options{};
