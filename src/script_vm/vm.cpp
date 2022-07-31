/**
 * zero-address code vm
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 8-jun-2022
 * @license see 'LICENSE' file
 */

#include "vm.h"

#include <iostream>
#include <sstream>
#include <cstring>


VM::VM(t_addr memsize, std::optional<t_addr> framesize)
	: m_memsize{memsize}, m_framesize{framesize ? *framesize : memsize/16}
{
	m_mem.reset(new t_byte[m_memsize]);
	Reset();
}


VM::~VM()
{
	StopTimer();
}


void VM::StartTimer()
{
	if(!m_timer_running)
	{
		m_timer_running = true;
		m_timer_thread = std::thread(&VM::TimerFunc, this);
	}
}


void VM::StopTimer()
{
	m_timer_running = false;
	if(m_timer_thread.joinable())
		m_timer_thread.join();
}


/**
 * function for timer thread
 */
void VM::TimerFunc()
{
	while(m_timer_running)
	{
		std::this_thread::sleep_for(m_timer_ticks);
		RequestInterrupt(m_timer_interrupt);
	}
}


/**
 * signals an interrupt
 */
void VM::RequestInterrupt(t_addr num)
{
	m_irqs[num] = true;
}


/**
 * sets the address of an interrupt service routine
 */
void VM::SetISR(t_addr num, t_addr addr)
{
	m_isrs[num] = addr;

	if(m_debug)
		std::cout << "Set isr " << num << " to address " << addr << "." << std::endl;
}


bool VM::Run()
{
	bool running = true;
	while(running)
	{
		CheckPointerBounds();
		if(m_drawmemimages)
			DrawMemoryImage();

		OpCode op{OpCode::INVALID};
		bool irq_active = false;

		// tests for interrupt requests
		for(t_addr irq=0; irq<m_num_interrupts; ++irq)
		{
			if(!m_irqs[irq])
				continue;

			m_irqs[irq] = false;
			if(!m_isrs[irq])
				continue;

			irq_active = true;

			// call interrupt service routine
			PushAddress(*m_isrs[irq], VMType::ADDR_MEM);
			op = OpCode::CALL;

			// TODO: add specialised ICALL and IRET instructions
			// in case of additional registers that might need saving
			break;
		}

		if(!irq_active)
		{
			t_byte _op = m_mem[m_ip++];
			op = static_cast<OpCode>(_op);
		}

		if(m_debug)
		{
			std::cout << "*** read instruction at ip = " << t_int(m_ip)
				<< ", sp = " << t_int(m_sp)
				<< ", bp = " << t_int(m_bp)
				<< ", gbp = " << t_int(m_gbp)
				<< ", opcode: " << std::hex
				<< static_cast<std::size_t>(op)
				<< " (" << get_vm_opcode_name(op) << ")"
				<< std::dec << ". ***" << std::endl;
		}

		// run instruction
		switch(op)
		{
			case OpCode::HALT:
			{
				running = false;
				break;
			}

			case OpCode::NOP:
			{
				break;
			}

			// push direct data onto stack
			case OpCode::PUSH:
			{
				auto [ty, val] = ReadMemData(m_ip);
				m_ip += GetDataSize(val) + m_bytesize;
				PushData(val, ty);
				break;
			}

			case OpCode::WRMEM:
			{
				// variable address
				t_addr addr = PopAddress();

				// pop data and write it to memory
				t_data val = PopData();
				WriteMemData(addr, val);
				break;
			}

			case OpCode::RDMEM:
			{
				// variable address
				t_addr addr = PopAddress();

				// read and push data from memory
				auto [ty, val] = ReadMemData(addr);
				PushData(val, ty);
				break;
			}

			case OpCode::USUB:
			{
				t_data val = PopData();
				t_data result;

				if(val.index() == m_realidx)
				{
					result = t_data{std::in_place_index<m_realidx>,
						-std::get<m_realidx>(val)};
				}
				else if(val.index() == m_intidx)
				{
					result = t_data{std::in_place_index<m_intidx>,
						-std::get<m_intidx>(val)};
				}
				else
				{
					throw std::runtime_error("Type mismatch in arithmetic operation.");
				}

				PushData(result);
				break;
			}

			case OpCode::ADD:
			{
				OpArithmetic<'+'>();
				break;
			}

			case OpCode::SUB:
			{
				OpArithmetic<'-'>();
				break;
			}

			case OpCode::MUL:
			{
				OpArithmetic<'*'>();
				break;
			}

			case OpCode::DIV:
			{
				OpArithmetic<'/'>();
				break;
			}

			case OpCode::MOD:
			{
				OpArithmetic<'%'>();
				break;
			}

			case OpCode::POW:
			{
				OpArithmetic<'^'>();
				break;
			}

			case OpCode::AND:
			{
				OpLogical<'&'>();
				break;
			}

			case OpCode::OR:
			{
				OpLogical<'|'>();
				break;
			}

			case OpCode::XOR:
			{
				OpLogical<'^'>();
				break;
			}

			case OpCode::NOT:
			{
				// might also use PopData and PushData in case ints
				// should also be allowed in boolean expressions
				t_bool val = PopRaw<t_bool, m_boolsize>();
				PushRaw<t_bool, m_boolsize>(!val);
				break;
			}

			case OpCode::BINAND:
			{
				OpBinary<'&'>();
				break;
			}

			case OpCode::BINOR:
			{
				OpBinary<'|'>();
				break;
			}

			case OpCode::BINXOR:
			{
				OpBinary<'^'>();
				break;
			}

			case OpCode::BINNOT:
			{
				t_data val = PopData();
				if(val.index() == m_intidx)
				{
					t_int newval = ~std::get<m_intidx>(val);
					PushData(t_data{std::in_place_index<m_intidx>, newval});
				}
				else
				{
					throw std::runtime_error("Invalid data type for binary not.");
				}

				break;
			}

			case OpCode::SHL:
			{
				OpBinary<'<'>();
				break;
			}

			case OpCode::SHR:
			{
				OpBinary<'>'>();
				break;
			}

			case OpCode::ROTL:
			{
				OpBinary<'l'>();
				break;
			}

			case OpCode::ROTR:
			{
				OpBinary<'r'>();
				break;
			}

			case OpCode::GT:
			{
				OpComparison<OpCode::GT>();
				break;
			}

			case OpCode::LT:
			{
				OpComparison<OpCode::LT>();
				break;
			}

			case OpCode::GEQU:
			{
				OpComparison<OpCode::GEQU>();
				break;
			}

			case OpCode::LEQU:
			{
				OpComparison<OpCode::LEQU>();
				break;
			}

			case OpCode::EQU:
			{
				OpComparison<OpCode::EQU>();
				break;
			}

			case OpCode::NEQU:
			{
				OpComparison<OpCode::NEQU>();
				break;
			}

			case OpCode::TOI: // converts value to t_int
			{
				OpCast<m_intidx>();
				break;
			}

			case OpCode::TOF: // converts value to t_real
			{
				OpCast<m_realidx>();
				break;
			}

			case OpCode::TOS: // converts value to t_str
			{
				OpCast<m_stridx>();
				break;
			}

			case OpCode::JMP: // jump to direct address
			{
				// get address from stack and set ip
				m_ip = PopAddress();
				break;
			}

			case OpCode::JMPCND: // conditional jump to direct address
			{
				// get address from stack
				t_addr addr = PopAddress();

				// get boolean condition result from stack
				t_bool cond = PopRaw<t_bool, m_boolsize>();

				// set instruction pointer
				if(cond)
					m_ip = addr;
				break;
			}

			/**
			 * stack frame for functions:
			 *
			 *  --------------------
			 * |  local var n       |  <-- m_sp
			 *  --------------------      |
			 * |      ...           |     |
			 *  --------------------      |
			 * |  local var 2       |     |  m_framesize
			 *  --------------------      |
			 * |  local var 1       |     |
			 *  --------------------      |
			 * |  old m_bp          |  <-- m_bp (= previous m_sp)
			 *  --------------------
			 * |  old m_ip for ret  |
			 *  --------------------
			 * |  func. arg 1       |
			 *  --------------------
			 * |  func. arg 2       |
			 *  --------------------
			 * |  ...               |
			 *  --------------------
			 * |  func. arg n       |
			 *  --------------------
			 */
			case OpCode::CALL: // function call
			{
				t_addr funcaddr = PopAddress();

				// save instruction and base pointer and
				// set up the function's stack frame for local variables
				PushAddress(m_ip, VMType::ADDR_MEM);
				PushAddress(m_bp, VMType::ADDR_MEM);

				if(m_debug)
				{
					std::cout << "saved base pointer "
						<< m_bp << "."
						<< std::endl;
				}
				m_bp = m_sp;
				m_sp -= m_framesize;

				// jump to function
				m_ip = funcaddr;
				if(m_debug)
				{
					std::cout << "calling function "
						<< funcaddr << "."
						<< std::endl;
				}
				break;
			}

			case OpCode::RET: // return from function
			{
				// get number of function arguments
				t_int num_args = std::get<m_intidx>(PopData());

				// if there's still a value on the stack, use it as return value
				t_data retval;
				if(m_sp + m_framesize < m_bp)
					retval = PopData();

				// zero the stack frame
				if(m_zeropoppedvals)
					std::memset(m_mem.get()+m_sp, 0, (m_bp-m_sp)*m_bytesize);

				// remove the function's stack frame
				m_sp = m_bp;

				m_bp = PopAddress();
				m_ip = PopAddress();  // jump back

				if(m_debug)
				{
					std::cout << "restored base pointer "
						<< m_bp << "."
						<< std::endl;
				}

				// remove function arguments from stack
				for(t_int arg=0; arg<num_args; ++arg)
					PopData();

				PushData(retval, VMType::UNKNOWN, false);
				break;
			}

			case OpCode::EXTCALL: // external function call
			{
				// get function name
				const t_str/*&*/ funcname = std::get<m_stridx>(PopData());

				t_data retval = CallExternal(funcname);
				PushData(retval, VMType::UNKNOWN, false);
				break;
			}

			default:
			{
				std::cerr << "Error: Invalid instruction " << std::hex
					<< static_cast<t_addr>(op) << std::dec
					<< std::endl;
				return false;
			}
		}

		// wrap around
		if(m_ip > m_memsize)
			m_ip %= m_memsize;
	}

	return true;
}


/**
 * pop an address from the stack
 * an address consists of the index of an register
 * holding the base address and an offset address
 */
VM::t_addr VM::PopAddress()
{
	// get register/type info from stack
	t_byte regval = PopRaw<t_byte, m_bytesize>();

	// get address from stack
	t_addr addr = PopRaw<t_addr, m_addrsize>();
	VMType thereg = static_cast<VMType>(regval);

	if(m_debug)
	{
		std::cout << "popped address " << t_int(addr)
			<< " of type " << t_int(regval)
			<< " (" << get_vm_type_name(thereg) << ")"
			<< "." << std::endl;
	}

	// get absolute address using base address from register
	switch(thereg)
	{
		case VMType::ADDR_MEM: break;
		case VMType::ADDR_IP: addr += m_ip; break;
		case VMType::ADDR_SP: addr += m_sp; break;
		case VMType::ADDR_BP: addr += m_bp; break;
		case VMType::ADDR_GBP: addr += m_gbp; break;
		//case VMType::ADDR_BP_ARG: break;
		default: throw std::runtime_error("Unknown address base register."); break;
	}

	return addr;
}


/**
 * push an address to stack
 */
void VM::PushAddress(t_addr addr, VMType ty)
{
	PushRaw<t_addr, m_addrsize>(addr);
	PushRaw<t_byte, m_bytesize>(static_cast<t_byte>(ty));
}


/**
 * pop a string from the stack
 * a string consists of an t_addr giving the length
 * following by the string (without 0-termination)
 */
VM::t_str VM::PopString()
{
	t_addr len = PopRaw<t_addr, m_addrsize>();
	CheckMemoryBounds(m_sp, len);

	t_char* begin = reinterpret_cast<t_char*>(m_mem.get() + m_sp);
	t_str str(begin, len);
	m_sp += len;

	if(m_zeropoppedvals)
		std::memset(begin, 0, len*m_bytesize);

	return str;
}


/**
 * get a string from the top of the stack
 */
VM::t_str VM::TopString(t_addr sp_offs) const
{
	t_addr len = TopRaw<t_addr, m_addrsize>(sp_offs);
	t_addr addr = m_sp + sp_offs + m_addrsize;

	CheckMemoryBounds(addr, len);
	t_char* begin = reinterpret_cast<t_char*>(m_mem.get() + addr);
	t_str str(begin, len);

	return str;
}


/**
 * push a string to the stack
 */
void VM::PushString(const VM::t_str& str)
{
	t_addr len = static_cast<t_addr>(str.length());
	CheckMemoryBounds(m_sp, len);

	m_sp -= len;
	t_char* begin = reinterpret_cast<t_char*>(m_mem.get() + m_sp);
	std::memcpy(begin, str.data(), len*sizeof(t_char));

	PushRaw<t_addr, m_addrsize>(len);
}


/**
 * get top data from the stack, which is prefixed
 * with a type descriptor byte
 */
VM::t_data VM::TopData() const
{
	// get data type info from stack
	t_byte tyval = TopRaw<t_byte, m_bytesize>();
	VMType ty = static_cast<VMType>(tyval);

	t_data dat;

	switch(ty)
	{
		case VMType::REAL:
		{
			dat = t_data{std::in_place_index<m_realidx>,
				TopRaw<t_real, m_realsize>(m_bytesize)};
			break;
		}

		case VMType::INT:
		{
			dat = t_data{std::in_place_index<m_intidx>,
				TopRaw<t_int, m_intsize>(m_bytesize)};
			break;
		}

		case VMType::ADDR_MEM:
		case VMType::ADDR_IP:
		case VMType::ADDR_SP:
		case VMType::ADDR_BP:
		case VMType::ADDR_GBP:
		{
			dat = t_data{std::in_place_index<m_addridx>,
				TopRaw<t_addr, m_addrsize>(m_bytesize)};
			break;
		}

		case VMType::STR:
		{
			dat = t_data{std::in_place_index<m_stridx>,
				TopString(m_bytesize)};
			break;
		}

		default:
		{
			std::ostringstream msg;
			msg << "Top: Data type " << (int)tyval
				<< " (" << get_vm_type_name(ty) << ")"
				<< " not yet implemented.";
			throw std::runtime_error(msg.str());
			break;
		}
	}

	return dat;
}


/**
 * pop data from the stack, which is prefixed
 * with a type descriptor byte
 */
VM::t_data VM::PopData()
{
	// get data type info from stack
	t_byte tyval = PopRaw<t_byte, m_bytesize>();
	VMType ty = static_cast<VMType>(tyval);

	t_data dat;

	switch(ty)
	{
		case VMType::REAL:
		{
			dat = t_data{std::in_place_index<m_realidx>,
				PopRaw<t_real, m_realsize>()};
			if(m_debug)
			{
				std::cout << "popped real " << std::get<m_realidx>(dat)
					<< "." << std::endl;
			}
			break;
		}

		case VMType::INT:
		{
			dat = t_data{std::in_place_index<m_intidx>,
				PopRaw<t_int, m_intsize>()};
			if(m_debug)
			{
				std::cout << "popped int " << std::get<m_intidx>(dat)
					<< "." << std::endl;
			}
			break;
		}

		case VMType::ADDR_MEM:
		case VMType::ADDR_IP:
		case VMType::ADDR_SP:
		case VMType::ADDR_BP:
		case VMType::ADDR_GBP:
		{
			dat = t_data{std::in_place_index<m_addridx>,
				PopRaw<t_addr, m_addrsize>()};
			if(m_debug)
			{
				std::cout << "popped address " << std::get<m_addridx>(dat)
					<< "." << std::endl;
			}
			break;
		}

		case VMType::STR:
		{
			dat = t_data{std::in_place_index<m_stridx>,
				PopString()};
			if(m_debug)
			{
				std::cout << "popped string \"" << std::get<m_stridx>(dat)
					<< "\"." << std::endl;
			}
			break;
		}

		default:
		{
			std::ostringstream msg;
			msg << "Pop: Data type " << (int)tyval
				<< " (" << get_vm_type_name(ty) << ")"
				<< " not yet implemented.";
			throw std::runtime_error(msg.str());
			break;
		}
	}

	return dat;
}


/**
 * push the raw data followed by a data type descriptor
 */
void VM::PushData(const VM::t_data& data, VMType ty, bool err_on_unknown)
{
	if(data.index() == m_realidx)
	{
		if(m_debug)
		{
			std::cout << "pushing real "
				<< std::get<m_realidx>(data) << "."
				<< std::endl;
		}

		// push the actual data
		PushRaw<t_real, m_realsize>(std::get<m_realidx>(data));

		// push descriptor
		PushRaw<t_byte, m_bytesize>(static_cast<t_byte>(VMType::REAL));
	}
	else if(data.index() == m_intidx)
	{
		if(m_debug)
		{
			std::cout << "pushing int "
				<< std::get<m_intidx>(data) << "."
				<< std::endl;
		}

		// push the actual data
		PushRaw<t_int, m_intsize>(std::get<m_intidx>(data));

		// push descriptor
		PushRaw<t_byte, m_bytesize>(static_cast<t_byte>(VMType::INT));
	}
	else if(data.index() == m_addridx)
	{
		if(m_debug)
		{
			std::cout << "pushing address "
				<< std::get<m_addridx>(data) << "."
				<< std::endl;
		}

		// push the actual address
		PushRaw<t_addr, m_addrsize>(std::get<m_addridx>(data));

		// push descriptor
		PushRaw<t_byte, m_bytesize>(static_cast<t_byte>(ty));
	}
	else if(data.index() == m_stridx)
	{
		if(m_debug)
		{
			std::cout << "pushing string \""
				<< std::get<m_stridx>(data) << "\"."
				<< std::endl;
		}

		// push the actual string
		PushString(std::get<m_stridx>(data));

		// push descriptor
		PushRaw<t_byte, m_bytesize>(static_cast<t_byte>(VMType::STR));
	}
	else if(err_on_unknown)
	{
		std::ostringstream msg;
		msg << "Push: Data type " << (int)ty
			<< " (" << get_vm_type_name(ty) << ")"
			<< " not yet implemented.";
		throw std::runtime_error(msg.str());
	}
}


/**
 * get the address of a function argument variable
 */
VM::t_addr VM::GetArgAddr(VM::t_addr addr, VM::t_addr arg_num) const
{
	// skip to correct argument index
	for(t_addr arg_idx = 0; arg_idx < arg_num; ++arg_idx)
	{
		// get data type info from memory
		VMType ty = static_cast<VMType>(ReadMemRaw<t_byte>(addr));
		addr += m_bytesize;

		switch(ty)
		{
			case VMType::REAL:
				addr += m_realsize;
				break;
			case VMType::INT:
				addr += m_intsize;
				break;
			case VMType::ADDR_MEM:
			case VMType::ADDR_IP:
			case VMType::ADDR_SP:
			case VMType::ADDR_BP:
			case VMType::ADDR_GBP:
				addr += m_addrsize;
				break;
			case VMType::STR:
			{
				t_addr len = ReadMemRaw<t_addr>(addr);
				addr += m_addrsize + len;
				break;
			}
			default:
			{
				std::ostringstream msg;
				msg << "GetArgAddr: Data type " << (int)ty
					<< " (" << get_vm_type_name(ty) << ")"
					<< " not yet implemented.";
				throw std::runtime_error(msg.str());
			}
		}
	}

	return addr;
}


/**
 * read type-prefixed data from memory
 */
std::tuple<VMType, VM::t_data> VM::ReadMemData(VM::t_addr addr)
{
	// get data type info from memory
	t_byte tyval = ReadMemRaw<t_byte>(addr);
	addr += m_bytesize;
	VMType ty = static_cast<VMType>(tyval);

	t_data dat;

	switch(ty)
	{
		case VMType::REAL:
		{
			t_real val = ReadMemRaw<t_real>(addr);
			dat = t_data{std::in_place_index<m_realidx>, val};

			if(m_debug)
			{
				std::cout << "read real " << val
					<< " from address " << (addr-1)
					<< "." << std::endl;
			}
			break;
		}

		case VMType::INT:
		{
			t_int val = ReadMemRaw<t_int>(addr);
			dat = t_data{std::in_place_index<m_intidx>, val};

			if(m_debug)
			{
				std::cout << "read int " << val
					<< " from address " << (addr-1)
					<< "." << std::endl;
			}
			break;
		}

		case VMType::ADDR_MEM:
		case VMType::ADDR_IP:
		case VMType::ADDR_SP:
		case VMType::ADDR_BP:
		case VMType::ADDR_GBP:
		{
			t_addr val = ReadMemRaw<t_addr>(addr);
			dat = t_data{std::in_place_index<m_addridx>, val};

			if(m_debug)
			{
				std::cout << "read address " << t_int(val)
					<< " from address " << t_int(addr-1)
					<< "." << std::endl;
			}
			break;
		}

		case VMType::ADDR_BP_ARG:
		{
			t_addr arg_num = ReadMemRaw<t_addr>(addr);
			t_addr arg_addr = GetArgAddr(m_bp, arg_num) - m_bp;
			ty = VMType::ADDR_BP;

			dat = t_data{std::in_place_index<m_addridx>, arg_addr};

			if(m_debug)
			{
				std::cout << "read address " << t_int(arg_addr)
					<< " for argument #" << t_int(arg_num)
					<< "." << std::endl;
			}

			// dereference:
			//std::tie(ty, dat, std::ignore) = ReadMemData(arg_addr);
			break;
		}

		case VMType::STR:
		{
			t_str str = ReadMemRaw<t_str>(addr);
			dat = t_data{std::in_place_index<m_stridx>, str};

			if(m_debug)
			{
				std::cout << "read string \"" << str
					<< "\" from address " << (addr-1)
					<< "." << std::endl;
			}
			break;
		}

		default:
		{
			std::ostringstream msg;
			msg << "ReadMem: Data type " << (int)tyval
				<< " (" << get_vm_type_name(ty) << ")"
				<< " not yet implemented.";
			throw std::runtime_error(msg.str());
			break;
		}
	}

	return std::make_tuple(ty, dat);
}


/**
 * write type-prefixed data to memory
 */
void VM::WriteMemData(VM::t_addr addr, const VM::t_data& data)
{
	if(data.index() == m_realidx)
	{
		if(m_debug)
		{
			std::cout << "writing real value "
				<< std::get<m_realidx>(data)
				<< " to address " << addr
				<< "." << std::endl;
		}

		// write descriptor prefix
		WriteMemRaw<t_byte>(addr, static_cast<t_byte>(VMType::REAL));
		addr += m_bytesize;

		// write the actual data
		WriteMemRaw<t_real>(addr, std::get<m_realidx>(data));
	}
	else if(data.index() == m_intidx)
	{
		if(m_debug)
		{
			std::cout << "writing int value "
				<< std::get<m_intidx>(data)
				<< " to address " << addr
				<< "." << std::endl;
		}

		// write descriptor prefix
		WriteMemRaw<t_byte>(addr, static_cast<t_byte>(VMType::INT));
		addr += m_bytesize;

		// write the actual data
		WriteMemRaw<t_int>(addr, std::get<m_intidx>(data));
	}
	/*else if(data.index() == m_addridx)
	{
		if(m_debug)
		{
			std::cout << "writing address value "
				<< std::get<m_addridx>(data)
				<< " to address " << addr
				<< std::endl;
		}

		// write descriptor prefix
		WriteMemRaw<t_byte>(addr, static_cast<t_byte>(ty));
		addr += m_bytesize;

		// write the actual data
		WriteMemRaw<t_int>(addr, std::get<m_addridx>(data));
	}*/
	else if(data.index() == m_stridx)
	{
		if(m_debug)
		{
			std::cout << "writing string \""
				<< std::get<m_stridx>(data)
				<< "\" to address " << addr
				<< "." << std::endl;
		}

		// write descriptor prefix
		WriteMemRaw<t_byte>(addr, static_cast<t_byte>(VMType::STR));
		addr += m_bytesize;

		// write the actual data
		WriteMemRaw<t_str>(addr, std::get<m_stridx>(data));
	}
	else
	{
		throw std::runtime_error("WriteMem: Data type not yet implemented.");
	}
}


VM::t_addr VM::GetDataSize(const t_data& data) const
{
	if(data.index() == m_realidx)
		return m_realsize;
	else if(data.index() == m_intidx)
		return m_intsize;
	else if(data.index() == m_addridx)
		return m_addrsize;
	else if(data.index() == m_stridx)
		return m_addrsize /*len*/ + std::get<m_stridx>(data).length();

	throw std::runtime_error("GetDataSize: Data type not yet implemented.");
	return 0;
}


void VM::Reset()
{
	m_ip = 0;
	m_sp = m_memsize - m_framesize;
	m_bp = m_memsize;
	m_bp -= sizeof(t_data) + 1; // padding of max. data type size to avoid writing beyond memory size
	m_gbp = m_bp;

	std::memset(m_mem.get(), static_cast<t_byte>(OpCode::HALT), m_memsize*m_bytesize);
	m_code_range[0] = m_code_range[1] = -1;
}


/**
 * sets or updates the range of memory where executable code resides
 */
void VM::UpdateCodeRange(t_addr begin, t_addr end)
{
	if(m_code_range[0] < 0 || m_code_range[1] < 0)
	{
		// set range
		m_code_range[0] = begin;
		m_code_range[1] = end;
	}
	else
	{
		// update range
		m_code_range[0] = std::min(m_code_range[0], begin);
		m_code_range[1] = std::max(m_code_range[1], end);
	}
}


void VM::SetMem(t_addr addr, VM::t_byte data)
{
	CheckMemoryBounds(addr, sizeof(t_byte));

	m_mem[addr % m_memsize] = data;
}


void VM::SetMem(t_addr addr, const t_str& data, bool is_code)
{
	if(is_code)
		UpdateCodeRange(addr, addr + data.size());

	for(std::size_t i=0; i<data.size(); ++i)
		SetMem(addr + (t_addr)(i), static_cast<t_byte>(data[i]));
}


void VM::SetMem(t_addr addr, const VM::t_byte* data, std::size_t size, bool is_code)
{
	if(is_code)
		UpdateCodeRange(addr, addr + size);

	for(std::size_t i=0; i<size; ++i)
		SetMem(addr + t_addr(i), data[i]);
}


const char* VM::GetDataTypeName(const t_data& dat)
{
	switch(dat.index())
	{
		case m_realidx: return "real";
		case m_intidx: return "integer";
		case m_stridx: return "string";
		case m_addridx: return "address";
		case m_boolidx: return "boolean";
		default: return "unknown";
	}
}


void VM::CheckMemoryBounds(t_addr addr, std::size_t size) const
{
	if(!m_checks)
		return;

	if(std::size_t(addr) + size > std::size_t(m_memsize) || addr < 0)
		throw std::runtime_error("Tried to access out of memory bounds.");
}


void VM::CheckPointerBounds() const
{
	if(!m_checks)
		return;

	// check code range?
	bool chk_c = (m_code_range[0] >= 0 && m_code_range[1] >= 0);

	if(m_ip > m_memsize || m_ip < 0 || (chk_c && (m_ip < m_code_range[0] || m_ip >= m_code_range[1])))
	{
		std::ostringstream msg;
		msg << "Instruction pointer " << t_int(m_ip) << " is out of memory bounds.";
		throw std::runtime_error(msg.str());
	}
	if(m_sp > m_memsize || m_sp < 0 || (chk_c && m_sp >= m_code_range[0] && m_sp < m_code_range[1]))
	{
		std::ostringstream msg;
		msg << "Stack pointer " << t_int(m_sp) << " is out of memory bounds.";
		throw std::runtime_error(msg.str());
	}
	if(m_bp > m_memsize || m_bp < 0 || (chk_c && m_bp >= m_code_range[0] && m_bp < m_code_range[1]))
	{
		std::ostringstream msg;
		msg << "Base pointer " << t_int(m_bp) << " is out of memory bounds.";
		throw std::runtime_error(msg.str());
	}
	if(m_gbp > m_memsize || m_gbp < 0 || (chk_c && m_gbp >= m_code_range[0] && m_gbp < m_code_range[1]))
	{
		std::ostringstream msg;
		msg << "Global base pointer " << t_int(m_gbp) << " is out of memory bounds.";
		throw std::runtime_error(msg.str());
	}
}
