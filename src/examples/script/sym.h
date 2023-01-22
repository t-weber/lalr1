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


	/**
	 * write a constant to the stream and get its position
	 */
	std::streampos AddConst(const t_constval& constval)
	{
		// look if the value is already in the map
		if(auto iter = m_consts.find(constval); iter != m_consts.end())
			return iter->second;

		std::streampos streampos = m_ostr.tellp();

		// write constant to stream
		if(std::holds_alternative<t_real>(constval))
		{
			const t_real realval = std::get<t_real>(constval);

			// write real type descriptor byte
			m_ostr.put(static_cast<t_vm_byte>(VMType::REAL));
			// write real data
			m_ostr.write(reinterpret_cast<const char*>(&realval),
				vm_type_size<VMType::REAL, false>);
		}
		else if(std::holds_alternative<t_int>(constval))
		{
			const t_int intval = std::get<t_int>(constval);

			// write int type descriptor byte
			m_ostr.put(static_cast<t_vm_byte>(VMType::INT));
			// write int data
			m_ostr.write(reinterpret_cast<const char*>(&intval),
				vm_type_size<VMType::INT, false>);
		}
		else if(std::holds_alternative<t_str>(constval))
		{
			const t_str& strval = std::get<t_str>(constval);

			// write string type descriptor byte
			m_ostr.put(static_cast<t_vm_byte>(VMType::STR));
			// write string length
			t_vm_addr len = static_cast<t_vm_addr>(strval.length());
			m_ostr.write(reinterpret_cast<const char*>(&len),
				vm_type_size<VMType::ADDR_MEM, false>);
			// write string data
			m_ostr.write(strval.data(), len);
		}
		else
		{
			throw std::runtime_error("Unknown constant type.");
		}

		// otherwise add a new constant to the map
		m_consts.insert(std::make_pair(constval, streampos));
		return streampos;
	}


	/**
	 * get the stream's bytes
	 */
	std::pair<std::streampos, std::shared_ptr<std::uint8_t[]>> GetBytes()
	{
		std::streampos size = m_ostr.tellp();
		if(!size)
			return std::make_pair(0, nullptr);

		auto buffer = std::make_shared<std::uint8_t[]>(size);
		m_ostr.seekg(0, std::ios_base::beg);

		m_ostr.read(reinterpret_cast<char*>(buffer.get()), size);
		return std::make_pair(size, buffer);
	}


private:
	std::unordered_map<t_constval, std::streampos> m_consts{};
	std::stringstream m_ostr{};
};



#endif
