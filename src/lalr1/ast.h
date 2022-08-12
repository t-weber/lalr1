/**
 * ast base for use with lalr(1) parser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 31-july-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_ASTBASE_H__
#define __LALR1_ASTBASE_H__

#include <memory>
#include <vector>
#include <functional>
#include <optional>



/**
 * syntax tree LALR(1) base
 */
class ASTLALR1Base
{
public:
	using t_line_range = std::pair<std::size_t, std::size_t>;


public:
	ASTLALR1Base(std::size_t id, std::optional<std::size_t> tableidx=std::nullopt)
		: m_id{id}, m_tableidx{tableidx}
	{}

	virtual ~ASTLALR1Base() = default;

	std::size_t GetId() const { return m_id; }
	void SetId(std::size_t id) { m_id = id; }

	std::size_t GetTableIdx() const { return *m_tableidx; }
	void SetTableIdx(std::size_t tableidx) { m_tableidx = tableidx; }

	virtual bool IsTerminal() const
	{
		if(m_isterminal)
			return *m_isterminal;
		return false;
	};

	std::optional<bool> GetTerminalOverride() const { return m_isterminal; }
	void SetTerminalOverride(bool b) { m_isterminal = b; }

	virtual const std::optional<t_line_range>& GetLineRange() const
	{ return m_line_range; }
	virtual void SetLineRange(const t_line_range& lines)
	{ m_line_range = lines; }
	virtual void SetLineRange(const std::optional<t_line_range>& lines)
	{ m_line_range = lines; }


private:
	// symbol id (from symbol.h)
	std::size_t m_id{};

	// index used in parse tables
	std::optional<std::size_t> m_tableidx{};

	// line number range
	std::optional<t_line_range> m_line_range{std::nullopt};

	// override terminal info
	std::optional<bool> m_isterminal{std::nullopt};
};


using t_lalrastbaseptr = std::shared_ptr<ASTLALR1Base>;


// semantic rule: returns an ast pointer and gets a vector of ast pointers
using t_semanticrule = std::function<
	t_lalrastbaseptr(const std::vector<t_lalrastbaseptr>&)>;


#endif
