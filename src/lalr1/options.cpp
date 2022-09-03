/**
 * global options
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 3-sep-2022
 * @license see 'LICENSE' file
 */

#include "options.h"


/**
 * print using colours
 */
void Options::SetUseColour(bool b)
{
	m_useColour = b;
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


Options g_options{};
