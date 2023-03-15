/**
 * ast base for use with lalr(1) parser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 31-july-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_ASTBASE_H__
#define __LALR1_ASTBASE_H__

#include <memory>
#include <deque>
#include <unordered_map>
#include <optional>
#include <functional>

#include <boost/functional/hash.hpp>

#include "types.h"


namespace lalr1 {

/**
 * syntax tree base
 */
class ASTBase
{
public:
	using t_line_range = std::pair<std::size_t, std::size_t>;


public:
	ASTBase(t_symbol_id id=t_symbol_id{},
		std::optional<t_index> tableidx=std::nullopt)
		: m_id{id}, m_tableidx{tableidx}
	{}

	virtual ~ASTBase() = default;

	t_symbol_id GetId() const { return m_id; }
	void SetId(t_symbol_id id) { m_id = id; }

	t_hash hash() const
	{
		t_hash hashTerm = std::hash<bool>{}(IsTerminal());
		t_hash hashId = std::hash<t_symbol_id>{}(GetId());

		t_hash hash = 0;
		boost::hash_combine(hash, hashTerm);
		boost::hash_combine(hash, hashId);
		return hash;
	}

	t_index GetTableIndex() const { return *m_tableidx; }
	void SetTableIndex(t_index tableidx) { m_tableidx = tableidx; }

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
	t_symbol_id m_id{};

	// index used in parse tables
	std::optional<t_index> m_tableidx{};

	// line number range
	std::optional<t_line_range> m_line_range{std::nullopt};

	// override terminal info
	std::optional<bool> m_isterminal{std::nullopt};
};


// symbol type for semantic rules
using t_astbaseptr = std::shared_ptr<ASTBase>;

// argument vector type to pass to semantic rules
using t_semanticargs = std::deque<t_astbaseptr>;

// semantic rule: returns an ast pointer and gets an argument vector
using t_semanticrule = std::function<
	t_astbaseptr(bool /*full_match*/,
	const t_semanticargs& /*args*/,
	t_astbaseptr /*retval*/ )>;

// map of semantic rules to their ids
using t_semanticrules = std::unordered_map<t_semantic_id, t_semanticrule>;

} // namespace lalr1

#endif
