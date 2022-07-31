/**
 * symbol table
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 10-jun-2022
 * @license see 'LICENSE' file
 */

#ifndef __LR1_SYMTAB_H__
#define __LR1_SYMTAB_H__


#include <unordered_map>
#include <ostream>
#include <iomanip>

#include "../script_vm/types.h"



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
 * symbol table mapping an identified to an address
 */
class SymTab
{
public:
	SymTab() {}
	~SymTab() {}


	const SymInfo* GetSymbol(const std::string& name) const
	{
		if(auto iter = m_syms.find(name); iter != m_syms.end())
			return &iter->second;

		return nullptr;
	}


	const SymInfo* AddSymbol(const std::string& name,
		t_vm_addr addr, VMType loc = VMType::ADDR_BP,
		VMType ty = VMType::UNKNOWN, bool is_func = false,
		t_vm_int num_args = 0)
	{
		SymInfo info
		{
			.addr = addr,
			.loc = loc,
			.ty = ty,
			.is_func = is_func,
			.num_args = num_args,
		};

		return &m_syms.insert_or_assign(name, info).first->second;
	}


	const std::unordered_map<std::string, SymInfo>& GetSymbols() const
	{
		return m_syms;
	}


	/**
	 * print symbol table
	 */
	friend std::ostream& operator<<(std::ostream& ostr, const SymTab& symtab)
	{
		std::ios_base::fmtflags base = ostr.flags(std::ios_base::basefield);

		const int len_name = 24;
		const int len_type = 24;
		const int len_addr = 14;
		const int len_base = 14;

		ostr
			<< std::setw(len_name) << std::left << "Name"
			<< std::setw(len_type) << std::left << "Type"
			<< std::setw(len_addr) << std::left << "Address"
			<< std::setw(len_base) << std::left << "Base"
			<< "\n";

		for(const auto& [name, info] : symtab.GetSymbols())
		{
			std::string ty = "<unknown>";
			if(info.is_func)
			{
				ty = "function, ";
				ty += std::to_string(info.num_args) + " args";
			}
			else
			{
				ty = get_vm_type_name(info.ty);
			}

			std::string basereg = get_vm_base_reg(info.loc);

			ostr
				<< std::setw(len_name) << std::left << name
				<< std::setw(len_type) << std::left << ty
				<< std::setw(len_addr) << std::left << std::dec << info.addr
				<< std::setw(len_base) << std::left << basereg
				<< "\n";
		}

		// restore base
		ostr.setf(base, std::ios_base::basefield);
		return ostr;
	}


private:
	std::unordered_map<std::string, SymInfo> m_syms{};
};


#endif
