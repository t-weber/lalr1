/**
 * zero-address code vm
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 8-jun-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_0ACVM_H__
#define __LALR1_0ACVM_H__


#include <type_traits>
#include <memory>
#include <array>
#include <optional>
#include <variant>
#include <iostream>
#include <sstream>
#include <bit>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <cstring>
#include <cmath>

#include "opcodes.h"
#include "helpers.h"


class VM
{
public:
	// data types
	using t_addr = ::t_vm_addr;
	using t_int = ::t_vm_int;
	using t_real = ::t_vm_real;
	using t_byte = ::t_vm_byte;
	using t_bool = ::t_vm_byte;
	using t_str = ::t_vm_str;
	using t_uint = typename std::make_unsigned<t_int>::type;
	using t_char = typename t_str::value_type;

	// variant of all data types
	using t_data = std::variant<
		std::monostate /*prevents default-construction of first type (t_real)*/,
		t_real, t_int, t_bool, t_addr, t_str>;

	// use variant type indices and std::in_place_index instead of direct types
	// because two types might be identical (e.g. t_int and t_addr)
	static constexpr const std::size_t m_realidx = 1;
	static constexpr const std::size_t m_intidx  = 2;
	static constexpr const std::size_t m_boolidx = 3;
	static constexpr const std::size_t m_addridx = 4;
	static constexpr const std::size_t m_stridx  = 5;

	// data type sizes
	static constexpr const t_addr m_bytesize = sizeof(t_byte);
	static constexpr const t_addr m_addrsize = sizeof(t_addr);
	static constexpr const t_addr m_realsize = sizeof(t_real);
	static constexpr const t_addr m_intsize = sizeof(t_int);
	static constexpr const t_addr m_boolsize = sizeof(t_bool);

	static constexpr const t_addr m_num_interrupts = 16;
	static constexpr const t_addr m_timer_interrupt = 0;


public:
	VM(t_addr memsize = 0x1000, std::optional<t_addr> framesize = std::nullopt);
	~VM();

	void SetDebug(bool b) { m_debug = b; }
	void SetDrawMemImages(bool b) { m_drawmemimages = b; }
	void SetChecks(bool b) { m_checks = b; }
	void SetZeroPoppedVals(bool b) { m_zeropoppedvals = b; }
	static const char* GetDataTypeName(const t_data& dat);

	void Reset();
	bool Run();

	void SetMem(t_addr addr, t_byte data);
	void SetMem(t_addr addr, const t_byte* data, std::size_t size, bool is_code = false);
	void SetMem(t_addr addr, const std::string& data, bool is_code = false);

	t_addr GetSP() const { return m_sp; }
	t_addr GetBP() const { return m_bp; }
	t_addr GetGBP() const { return m_gbp; }
	t_addr GetIP() const { return m_ip; }

	void SetSP(t_addr sp) { m_sp = sp; }
	void SetBP(t_addr bp) { m_bp = bp; }
	void SetGBP(t_addr bp) { m_gbp = bp; }
	void SetIP(t_addr ip) { m_ip = ip; }

	/**
	 * get top data from the stack
	 */
	t_data TopData() const;

	/**
	 * pop data from the stack
	 */
	t_data PopData();

	/**
	 * signals an interrupt
	 */
	void RequestInterrupt(t_addr num);

	/**
	 * visualises vm memory utilisation
	 */
	void DrawMemoryImage();


protected:
	/**
	 * return the size of the held data
	 */
	t_addr GetDataSize(const t_data& data) const;


	/**
	 * call external function
	 */
	t_data CallExternal(const t_str& func_name);


	/**
	 * pop an address from the stack
	 */
	t_addr PopAddress();


	/**
	 * push an address to stack
	 */
	void PushAddress(t_addr addr, VMType ty = VMType::ADDR_MEM);

	/**
	 * pop a string from the stack
	 */
	t_str PopString();

	/**
	 * get the string on top of the stack
	 */
	t_str TopString(t_addr sp_offs = 0) const;

	/**
	 * push a string to the stack
	 */
	void PushString(const t_str& str);

	/**
	 * push data onto the stack
	 */
	void PushData(const t_data& data, VMType ty = VMType::UNKNOWN, bool err_on_unknown = true);


	/**
	 * get the address of a function argument variable
	 */
	VM::t_addr GetArgAddr(t_addr addr, t_addr arg_num) const;


	/**
	 * read data from memory
	 */
	std::tuple<VMType, t_data> ReadMemData(t_addr addr);

	/**
	 * write data to memory
	 */
	void WriteMemData(t_addr addr, const t_data& data);


	/**
	 * read a raw value from memory
	 */
	template<class t_val>
	t_val ReadMemRaw(t_addr addr) const
	{
		// string type
		if constexpr(std::is_same_v<std::decay_t<t_val>, t_str>)
		{
			t_addr len = ReadMemRaw<t_addr>(addr);
			addr += m_addrsize;

			CheckMemoryBounds(addr, len);
			t_char* begin = reinterpret_cast<t_char*>(&m_mem[addr]);

			t_str str(begin, len);
			return str;
		}

		// primitive types
		else
		{
			CheckMemoryBounds(addr, sizeof(t_val));
			t_val val = *reinterpret_cast<t_val*>(&m_mem[addr]);

			return val;
		}
	}


	/**
	 * write a raw value to memory
	 */
	template<class t_val>
	void WriteMemRaw(t_addr addr, const t_val& val)
	{
		// string type
		if constexpr(std::is_same_v<std::decay_t<t_val>, t_str>)
		{
			t_addr len = static_cast<t_addr>(val.length());
			CheckMemoryBounds(addr, m_addrsize + len);

			// write string length
			WriteMemRaw<t_addr>(addr, len);
			addr += m_addrsize;

			// write string
			t_char* begin = reinterpret_cast<t_char*>(&m_mem[addr]);
			std::memcpy(begin, val.data(), len*sizeof(t_char));
		}

		// primitive types
		else
		{
			CheckMemoryBounds(addr, sizeof(t_val));

			*reinterpret_cast<t_val*>(&m_mem[addr]) = val;
		}
	}


	/**
	 * get the value on top of the stack
	 */
	template<class t_val, t_addr valsize = sizeof(t_val)>
	t_val TopRaw(t_addr sp_offs = 0) const
	{
		t_addr addr = m_sp + sp_offs;
		CheckMemoryBounds(addr, valsize);

		return *reinterpret_cast<t_val*>(m_mem.get() + addr);
	}


	/**
	 * pop a raw value from the stack
	 */
	template<class t_val, t_addr valsize = sizeof(t_val)>
	t_val PopRaw()
	{
		CheckMemoryBounds(m_sp, valsize);

		t_val *valptr = reinterpret_cast<t_val*>(m_mem.get() + m_sp);
		t_val val = *valptr;

		if(m_zeropoppedvals)
			*valptr = 0;

		m_sp += valsize;	// stack grows to lower addresses

		return val;
	}


	/**
	 * push a raw value onto the stack
	 */
	template<class t_val, t_addr valsize = sizeof(t_val)>
	void PushRaw(const t_val& val)
	{
		CheckMemoryBounds(m_sp, valsize);

		m_sp -= valsize;	// stack grows to lower addresses
		*reinterpret_cast<t_val*>(m_mem.get() + m_sp) = val;
	}


	/**
	 * cast from one variable type to the other
	 */
	template<std::size_t toidx>
	void OpCast()
	{
		using t_to = std::variant_alternative_t<toidx, t_data>;
		t_data data = TopData();

		if(data.index() == m_realidx)
		{
			if constexpr(std::is_same_v<std::decay_t<t_to>, t_real>)
				return;  // don't need to cast to the same type

			t_real val = std::get<m_realidx>(data);

			// convert to string
			if constexpr(std::is_same_v<std::decay_t<t_to>, t_str>)
			{
				std::ostringstream ostr;
				ostr << val;
				PopData();
				PushData(t_data{std::in_place_index<m_stridx>,
					ostr.str()});
			}

			// convert to primitive type
			else
			{
				PopData();
				PushData(t_data{std::in_place_index<toidx>,
					static_cast<t_to>(val)});
			}
		}
		else if(data.index() == m_intidx)
		{
			if constexpr(std::is_same_v<std::decay_t<t_to>, t_int>)
				return;  // don't need to cast to the same type

			t_int val = std::get<m_intidx>(data);

			// convert to string
			if constexpr(std::is_same_v<std::decay_t<t_to>, t_str>)
			{
				std::ostringstream ostr;
				ostr << val;
				PopData();
				PushData(t_data{std::in_place_index<m_stridx>,
					ostr.str()});
			}

			// convert to primitive type
			else
			{
				PopData();
				PushData(t_data{std::in_place_index<toidx>,
					static_cast<t_to>(val)});
			}
		}
		else if(data.index() == m_stridx)
		{
			if constexpr(std::is_same_v<std::decay_t<t_to>, t_str>)
				return;  // don't need to cast to the same type

			const t_str& val = std::get<m_stridx>(data);

			t_to conv_val{};
			std::istringstream{val} >> conv_val;
			PopData();
			PushData(t_data{std::in_place_index<toidx>,
				conv_val});
		}
	}


	/**
	 * arithmetic operation
	 */
	template<class t_val, char op>
	t_val OpArithmetic(const t_val& val1, const t_val& val2)
	{
		t_val result{};

		// string operators
		if constexpr(std::is_same_v<std::decay_t<t_val>, t_str>)
		{
			if constexpr(op == '+')
				result = val1 + val2;
		}

		// int / real operators
		else
		{
			if constexpr(op == '+')
				result = val1 + val2;
			else if constexpr(op == '-')
				result = val1 - val2;
			else if constexpr(op == '*')
				result = val1 * val2;
			else if constexpr(op == '/')
				result = val1 / val2;
			else if constexpr(op == '%' && std::is_integral_v<t_val>)
				result = val1 % val2;
			else if constexpr(op == '%' && std::is_floating_point_v<t_val>)
				result = std::fmod(val1, val2);
			else if constexpr(op == '^' && std::is_integral_v<t_val>)
				result = pow<t_val>(val1, val2);
			else if constexpr(op == '^' && std::is_floating_point_v<t_val>)
				result = pow<t_val>(val1, val2);
		}

		return result;
	}


	/**
	 * arithmetic operation
	 */
	template<char op>
	void OpArithmetic()
	{
		t_data val2 = PopData();
		t_data val1 = PopData();

		if(val1.index() != val2.index())
		{
			throw std::runtime_error("Type mismatch in arithmetic operation. "
				"Type indices: " + std::to_string(val1.index()) +
					", " + std::to_string(val2.index()) + ".");
		}

		t_data result;

		if(val1.index() == m_realidx)
		{
			result = t_data{std::in_place_index<m_realidx>, OpArithmetic<t_real, op>(
				std::get<m_realidx>(val1), std::get<m_realidx>(val2))};
		}
		else if(val1.index() == m_intidx)
		{
			result = t_data{std::in_place_index<m_intidx>, OpArithmetic<t_int, op>(
				std::get<m_intidx>(val1), std::get<m_intidx>(val2))};
		}
		else if(val1.index() == m_stridx)
		{
			result = t_data{std::in_place_index<m_stridx>, OpArithmetic<t_str, op>(
				std::get<m_stridx>(val1), std::get<m_stridx>(val2))};
		}
		else
		{
			throw std::runtime_error("Invalid type in arithmetic operation.");
		}

		PushData(result);
	}


	/**
	 * logical operation
	 */
	template<char op>
	void OpLogical()
	{
		// might also use PopData and PushData in case ints
		// should also be allowed in boolean expressions
		t_bool val2 = PopRaw<t_bool, m_boolsize>();
		t_bool val1 = PopRaw<t_bool, m_boolsize>();

		t_bool result = 0;

		if constexpr(op == '&')
			result = val1 && val2;
		else if constexpr(op == '|')
			result = val1 || val2;
		else if constexpr(op == '^')
			result = val1 ^ val2;

		PushRaw<t_bool, m_boolsize>(result);
	}


	/**
	 * binary operation
	 */
	template<class t_val, char op>
	t_val OpBinary(const t_val& val1, const t_val& val2)
	{
		t_val result{};

		// int operators
		if constexpr(std::is_same_v<std::decay_t<t_int>, t_int>)
		{
			if constexpr(op == '&')
				result = val1 & val2;
			else if constexpr(op == '|')
				result = val1 | val2;
			else if constexpr(op == '^')
				result = val1 ^ val2;
			else if constexpr(op == '<')  // left shift
				result = val1 << val2;
			else if constexpr(op == '>')  // right shift
				result = val1 >> val2;
			else if constexpr(op == 'l')  // left rotation
				result = static_cast<t_int>(std::rotl<t_uint>(val1, static_cast<int>(val2)));
			else if constexpr(op == 'r')  // right rotation
				result = static_cast<t_int>(std::rotr<t_uint>(val1, static_cast<int>(val2)));
		}

		return result;
	}


	/**
	 * binary operation
	 */
	template<char op>
	void OpBinary()
	{
		t_data val2 = PopData();
		t_data val1 = PopData();

		if(val1.index() != val2.index())
		{
			throw std::runtime_error("Type mismatch in binary operation. "
				"Type indices: " + std::to_string(val1.index()) +
					", " + std::to_string(val2.index()) + ".");
		}

		t_data result;

		if(val1.index() == m_intidx)
		{
			result = t_data{std::in_place_index<m_intidx>, OpBinary<t_int, op>(
				std::get<m_intidx>(val1), std::get<m_intidx>(val2))};
		}
		else
		{
			throw std::runtime_error("Invalid type in binary operation.");
		}

		PushData(result);
	}


	/**
	 * comparison operation
	 */
	template<class t_val, OpCode op>
	t_bool OpComparison(const t_val& val1, const t_val& val2)
	{
		t_bool result = 0;

		// string comparison
		if constexpr(std::is_same_v<std::decay_t<t_val>, t_str>)
		{
			if constexpr(op == OpCode::EQU)
				result = (val1 == val2);
			else if constexpr(op == OpCode::NEQU)
				result = (val1 != val2);
		}

		// integer /  real comparison
		else
		{
			if constexpr(op == OpCode::GT)
				result = (val1 > val2);
			else if constexpr(op == OpCode::LT)
				result = (val1 < val2);
			else if constexpr(op == OpCode::GEQU)
				result = (val1 >= val2);
			else if constexpr(op == OpCode::LEQU)
				result = (val1 <= val2);
			else if constexpr(op == OpCode::EQU)
			{
				if constexpr(std::is_same_v<std::decay_t<t_val>, t_real>)
					result = (std::abs(val1 - val2) <= m_eps);
				else
					result = (val1 == val2);
			}
			else if constexpr(op == OpCode::NEQU)
			{
				if constexpr(std::is_same_v<std::decay_t<t_val>, t_real>)
					result = (std::abs(val1 - val2) > m_eps);
				else
					result = (val1 != val2);
			}
		}

		return result;
	}


	/**
	 * comparison operation
	 */
	template<OpCode op>
	void OpComparison()
	{
		t_data val2 = PopData();
		t_data val1 = PopData();

		if(val1.index() != val2.index())
		{
			throw std::runtime_error("Type mismatch in comparison operation. "
				"Type indices: " + std::to_string(val1.index()) +
					", " + std::to_string(val2.index()) + ".");
		}

		t_bool result;

		if(val1.index() == m_realidx)
		{
			result = OpComparison<t_real, op>(
				std::get<m_realidx>(val1), std::get<m_realidx>(val2));
		}
		else if(val1.index() == m_intidx)
		{
			result = OpComparison<t_int, op>(
				std::get<m_intidx>(val1), std::get<m_intidx>(val2));
		}
		else if(val1.index() == m_stridx)
		{
			result = OpComparison<t_str, op>(
				std::get<m_stridx>(val1), std::get<m_stridx>(val2));
		}
		else
		{
			throw std::runtime_error("Invalid type in comparison operation.");
		}

		PushRaw<t_bool, m_boolsize>(result);
	}


	/**
	 * sets the address of an interrupt service routine
	 */
	void SetISR(t_addr num, t_addr addr);

	void StartTimer();
	void StopTimer();


private:
	void CheckMemoryBounds(t_addr addr, std::size_t size = 1) const;
	void CheckPointerBounds() const;
	void UpdateCodeRange(t_addr begin, t_addr end);

	void TimerFunc();


private:
	bool m_debug{false};               // write debug messages
	bool m_checks{true};               // do memory boundary checks
	bool m_drawmemimages{false};       // write memory dump images
	bool m_zeropoppedvals{false};      // zero memory of popped values
	t_real m_eps{std::numeric_limits<t_real>::epsilon()};

	std::unique_ptr<t_byte[]> m_mem{}; // ram
	t_addr m_code_range[2]{-1, -1};    // address range where the code resides

	// registers
	t_addr m_ip{};                     // instruction pointer
	t_addr m_sp{};                     // stack pointer
	t_addr m_bp{};                     // base pointer for local variables
	t_addr m_gbp{};                    // global base pointer

	// memory sizes and ranges
	t_addr m_memsize = 0x1000;         // total memory size
	t_addr m_framesize = 0x100;        // size per function stack frame

	// signals interrupt requests
	std::array<std::atomic_bool, m_num_interrupts> m_irqs{};
	// addresses of the interrupt service routines
	std::array<std::optional<t_addr>, m_num_interrupts> m_isrs{};

	std::thread m_timer_thread{};
	bool m_timer_running{false};
	std::chrono::milliseconds m_timer_ticks{250};
};


#endif
