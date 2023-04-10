/**
 * symbol and constants tables
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 10-jun-2022
 * @license see 'LICENSE' file
 */

#ifndef __LR1_SYMTAB_H__
#define __LR1_SYMTAB_H__

#include <unordered_map>
#include <variant>
#include <memory>
#include <ostream>
#include <sstream>
#include <iomanip>

#include "lval.h"
#include "script_vm/types.h"


/**
 * information about a variable in the symbol table
 */
struct SymInfo
{
	t_vm_addr addr{0};             // relative address of the variable
	VMType loc{VMType::ADDR_BP};   // register with the base address

	VMType ty{VMType::UNKNOWN};    // data type
	bool is_func{false};           // function or variable

	t_vm_int num_args{0};          // number of arguments
};


/**
 * symbol table mapping an identifier to an address
 */
class SymTab
{
public:
	SymTab() = default;
	~SymTab() = default;

	const SymInfo* GetSymbol(const std::string& name) const;

	const SymInfo* AddSymbol(const std::string& name,
		t_vm_addr addr, VMType loc = VMType::ADDR_BP,
		VMType ty = VMType::UNKNOWN, bool is_func = false,
		t_vm_int num_args = 0);

	const std::unordered_map<std::string, SymInfo>& GetSymbols() const;

	friend std::ostream& operator<<(std::ostream& ostr, const SymTab& symtab);


private:
	std::unordered_map<std::string, SymInfo> m_syms{};
};



/**
 * constants table
 */
class ConstTab
{
public:
	// possible constant types
	using t_constval = std::variant<std::monostate, t_real, t_int, t_str>;


public:
	ConstTab() = default;
	~ConstTab() = default;

	// write a constant to the stream and get its position
	std::streampos AddConst(const t_constval& constval);

	// get the stream's bytes
	std::pair<std::streampos, std::shared_ptr<std::uint8_t/*[]*/>> GetBytes();


private:
	std::unordered_map<t_constval, std::streampos> m_consts{};
	std::stringstream m_ostr{};
};


#endif
