/**
 * global options
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 3-sep-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_OPTIONS_H__
#define __LALR1_OPTIONS_H__

#include <string>

namespace lalr1 {

class Options
{
public:
	void SetUseColour(bool b = true);
	bool GetUseColour() const;

	void SetUseAsciiChars(bool b = true);

	const std::string& GetShiftColour() const;
	const std::string& GetReduceColour() const;
	const std::string& GetJumpColour() const;

	const std::string& GetTermShiftColour() const;
	const std::string& GetTermReduceColour() const;
	const std::string& GetTermJumpColour() const;
	const std::string& GetTermNoColour() const;
	const std::string& GetTermBoldColour() const;

	const std::string& GetCursorChar() const;
	const std::string& GetArrowChar() const;
	const std::string& GetSeparatorChar() const;
	const std::string& GetEpsilonChar() const;
	const std::string& GetEndChar() const;


private:
	// flags
	bool m_useColour{true};
	bool m_useAsciiChars{false};

	// colours
	std::string m_shift_col{"#ff0000"};
	std::string m_reduce_col{"#007700"};
	std::string m_jump_col{"#0000ff"};

	// terminal colour codes
	std::string m_term_shift_col{"\x1b[1;31m"};
	std::string m_term_reduce_col{"\x1b[1;32m"};
	std::string m_term_jump_col{"\x1b[1;34m"};
	std::string m_term_no_col{"\x1b[0m"};
	std::string m_term_bold_col{"\x1b[1m"};

	// utf-8 variant of special characters
	std::string m_cursor{"\xe2\x80\xa2"};
	std::string m_arrow{"\xe2\x86\x92"};
	std::string m_sep{"\xef\xbd\x9c"};
	std::string m_eps{"\xce\xb5"};
	std::string m_end{"\xcf\x89"};

	// ascii variant of special characters
	std::string m_cursor_asc{"."};
	std::string m_arrow_asc{"->"};
	std::string m_sep_asc{"|"};
	std::string m_eps_asc{"eps"};
	std::string m_end_asc{"$"};
};


extern Options g_options;

} // namespace lalr1

#endif
