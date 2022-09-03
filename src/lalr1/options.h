/**
 * global options
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 3-sep-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_OPTIONS_H__
#define __LALR1_OPTIONS_H__

#include <string>


class Options
{
public:
	void SetUseColour(bool b = true);
	bool GetUseColour() const;

	const std::string& GetShiftColour() const;
	const std::string& GetReduceColour() const;
	const std::string& GetJumpColour() const;

	const std::string& GetTermShiftColour() const;
	const std::string& GetTermReduceColour() const;
	const std::string& GetTermJumpColour() const;
	const std::string& GetTermNoColour() const;
	const std::string& GetTermBoldColour() const;


private:
	bool m_useColour{true};

	std::string m_shift_col{"#ff0000"};
	std::string m_reduce_col{"#007700"};
	std::string m_jump_col{"#0000ff"};

	std::string m_term_shift_col{"\x1b[1;31m"};
	std::string m_term_reduce_col{"\x1b[1;32m"};
	std::string m_term_jump_col{"\x1b[1;34m"};
	std::string m_term_no_col{"\x1b[0m"};
	std::string m_term_bold_col{"\x1b[1m"};
};


extern Options g_options;


#endif
