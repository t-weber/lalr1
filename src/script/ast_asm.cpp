/**
 * zero-address asm code generator
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14-jun-2022
 * @license see 'LICENSE' file
 */

#include "ast_asm.h"
#include <cmath>

#define AST_ABS_FUNC_ADDR  0  // use absolute or relative function addresses


/**
 * exit with an error
 */
static void throw_err(const ASTBase* ast, const std::string& err)
{
	if(ast)
	{
		std::ostringstream ostr;

		if(auto line_range = ast->GetLineRange(); line_range)
		{
			auto start_line = std::get<0>(*line_range);
			auto end_line = std::get<1>(*line_range);

			if(start_line == end_line)
				ostr << "Line " << start_line << ": ";
			else
				ostr << "Lines " << start_line << "..." << end_line << ": ";

			ostr << err;
		}

		throw std::runtime_error(ostr.str());
	}
	else
	{
		throw std::runtime_error(err);
	}
}



ASTAsm::ASTAsm(std::ostream& ostr,
	std::unordered_map<std::size_t, std::tuple<std::string, OpCode>> *ops)
	: m_ostr{&ostr}, m_ops{ops}
{
}


void ASTAsm::visit(
	[[maybe_unused]] const ASTToken<t_lval>* ast,
	[[maybe_unused]] std::size_t level)
{
	std::cerr << "Error: " << __func__ << " not implemented." << std::endl;
}


void ASTAsm::visit(const ASTToken<t_real>* ast,
	[[maybe_unused]] std::size_t level)
{
	if(!ast->HasLexerValue())
		return;
	t_vm_real val = static_cast<t_vm_real>(ast->GetLexerValue());

	if(m_binary)
	{
		//m_ostr->put(std::to_underlying(OpCode::PUSH));
		m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH));
		// write type descriptor byte
		m_ostr->put(static_cast<t_vm_byte>(VMType::REAL));
		// write value
		m_ostr->write(reinterpret_cast<const char*>(&val),
			vm_type_size<VMType::REAL, false>);
	}
	else
	{
		(*m_ostr) << "push real " << val << std::endl;
	}
}


void ASTAsm::visit(const ASTToken<t_int>* ast,
	[[maybe_unused]] std::size_t level)
{
	if(!ast->HasLexerValue())
		return;
	t_vm_int val = static_cast<t_vm_int>(ast->GetLexerValue());

	if(m_binary)
	{
		//m_ostr->put(std::to_underlying(OpCode::PUSH));
		m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH));
		// write type descriptor byte
		m_ostr->put(static_cast<t_vm_byte>(VMType::INT));
		// write data
		m_ostr->write(reinterpret_cast<const char*>(&val),
			vm_type_size<VMType::INT, false>);
	}
	else
	{
		(*m_ostr) << "push int " << val << std::endl;
	}
}


void ASTAsm::visit(const ASTToken<std::string>* ast,
	[[maybe_unused]] std::size_t level)
{
	if(!ast->HasLexerValue())
		return;

	const std::string& val = ast->GetLexerValue();

	if(m_binary)
	{
		// the token names a variable identifier
		if(ast->IsIdent())
		{
			std::string varname;
			if(m_cur_func != "")
				varname = m_cur_func + "/" + val;
			else
				varname = val;

			// get variable address and push it
			const SymInfo *sym = m_symtab.GetSymbol(varname);
			// symbol not yet seen -> register it
			if(!sym)
			{
				VMType symty = ast->GetDataType();

				// in global scope
				if(m_cur_func == "")
				{
					sym = m_symtab.AddSymbol(varname, -m_glob_stack,
						VMType::ADDR_GBP, symty);
					m_glob_stack += get_vm_type_size(symty, true);

					//std::cout << "added global symbol \""
					//	<< varname << "\" with size "
					//	<< get_vm_type_size(symty, true)
					//	<< std::endl;
				}

				// in local function scope
				else
				{
					m_local_stack.try_emplace(m_cur_func, 0);

					m_local_stack[m_cur_func] += get_vm_type_size(symty, true);
					sym = m_symtab.AddSymbol(varname, -m_local_stack[m_cur_func],
						VMType::ADDR_BP, symty);

					//std::cout << "added local symbol \""
					//	<< varname << "\" with size "
					//	<< get_vm_type_size(symty, true)
					//	<< std::endl;
				}
			}

			//std::cout << "pushing " << get_vm_type_name(sym->loc) << " "
			//	<< int(sym->addr) << " of symbol " << "\"" << varname << "\""
			//	<< "." << std::endl;

			m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH));
			// register with base address
			m_ostr->put(static_cast<t_vm_byte>(sym->loc));
			// relative address
			m_ostr->write(reinterpret_cast<const char*>(&sym->addr),
				vm_type_size<VMType::ADDR_BP, false>);

			// dereference it, if the variable is on the rhs of an assignment
			if(!ast->IsLValue() && !sym->is_func)
				m_ostr->put(static_cast<t_vm_byte>(OpCode::RDMEM));
		}

		// the token names a string literal
		else
		{
			m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH));
			// write type descriptor byte
			m_ostr->put(static_cast<t_vm_byte>(VMType::STR));
			// write string length
			t_vm_addr len = static_cast<t_vm_addr>(val.length());
			m_ostr->write(reinterpret_cast<const char*>(&len),
				vm_type_size<VMType::ADDR_MEM, false>);
			// write string data
			m_ostr->write(val.data(), len);
		}
	}
	else
	{
		// the token names a variable identifier
		if(ast->IsIdent())
		{
			(*m_ostr) << "push addr " << val << std::endl;

			// dereference it, if the variable is on the rhs of an assignment
			if(!ast->IsLValue())
				(*m_ostr) << "rdmem" << std::endl;
		}

		// the token names a string literal
		else
		{
			(*m_ostr) << "push string \"" << val << "\"" << std::endl;
		}
	}
}


void ASTAsm::visit(
	[[maybe_unused]] const ASTToken<void*>* ast,
	[[maybe_unused]] std::size_t level)
{
	std::cerr << "Error: " << __func__ << " not implemented." << std::endl;
}


void ASTAsm::visit(const ASTUnary* ast, [[maybe_unused]] std::size_t level)
{
	ast->GetChild(0)->accept(this, level+1);

	std::size_t opid = ast->GetOpId();

	if(m_binary)
	{
		OpCode op = std::get<OpCode>(m_ops->at(opid));
		if(op == OpCode::ADD)
			op = OpCode::NOP;
		else if(op == OpCode::SUB)
			op = OpCode::USUB;
		m_ostr->put(static_cast<t_vm_byte>(op));
	}
	else
	{
		if(m_ops)
			(*m_ostr) << "u" << std::get<std::string>(m_ops->at(opid));
		else
			(*m_ostr) << static_cast<char>(opid);
		(*m_ostr) << std::endl;
	}
}


void ASTAsm::visit(const ASTBinary* ast, [[maybe_unused]] std::size_t level)
{
	std::size_t opid = ast->GetOpId();
	VMType ty = ast->GetDataType();

	// iterate the operands
	for(std::size_t childidx=0; childidx<2; ++childidx)
	{
		auto child = ast->GetChild(childidx);
		child->accept(this, level+1);

		VMType subty = child->GetDataType();
		if(subty != ty && opid != '=' /* no cast on assignments */)
		{
			// child type is different from derived type -> cast

			if(m_binary)
			{
				if(ty == VMType::STR)
					m_ostr->put(static_cast<t_vm_byte>(OpCode::TOS));
				else if(ty == VMType::INT)
					m_ostr->put(static_cast<t_vm_byte>(OpCode::TOI));
				else if(ty == VMType::REAL)
					m_ostr->put(static_cast<t_vm_byte>(OpCode::TOF));
			}
			else
			{
				if(ty == VMType::STR)
					(*m_ostr) << "tos\n";
				else if(ty == VMType::INT)
					(*m_ostr) << "toi\n";
				else if(ty == VMType::REAL)
					(*m_ostr) << "tof\n";
			}
		}
	}

	// generate the binary operation
	if(m_binary)
	{
		OpCode op = std::get<OpCode>(m_ops->at(opid));
		if(op != OpCode::INVALID)	// use opcode directly
		{
			m_ostr->put(static_cast<t_vm_byte>(op));
		}
		else
		{
			// TODO: decide on special cases
		}
	}
	else
	{
		if(m_ops)
			(*m_ostr) << std::get<std::string>(m_ops->at(opid));
		else
			(*m_ostr) << static_cast<char>(opid);
		(*m_ostr) << std::endl;
	}
}


void ASTAsm::visit(const ASTList* ast, [[maybe_unused]] std::size_t level)
{
	for(std::size_t i=0; i<ast->NumChildren(); ++i)
		ast->GetChild(i)->accept(this, level+1);
}


void ASTAsm::visit(const ASTCondition* ast, [[maybe_unused]] std::size_t level)
{
	// condition
	ast->GetCondition()->accept(this, level+1);

	std::size_t labelEndCond = m_glob_label++;
	std::size_t labelEndIf = m_glob_label++;

	t_vm_addr skipEndCond = 0;         // how many bytes to skip to jump to end of the if block?
	t_vm_addr skipEndIf = 0;           // how many bytes to skip to jump to end of the entire if statement?
	std::streampos skip_addr = 0;      // stream position with the condition jump label
	std::streampos skip_else_addr = 0; // stream position with the if block jump label

	if(m_binary)
	{
		// if the condition is not fulfilled...
		m_ostr->put(static_cast<t_vm_byte>(OpCode::NOT));

		// ...skip to the end of the if block
		m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH)); // push jump address
		m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_IP));
		skip_addr = m_ostr->tellp();
		m_ostr->write(reinterpret_cast<const char*>(&skipEndCond),
			vm_type_size<VMType::ADDR_IP, false>);
		m_ostr->put(static_cast<t_vm_byte>(OpCode::JMPCND));
	}
	else
	{
		(*m_ostr) << "not\n";
		(*m_ostr) << "push addr end_cond_" << labelEndCond << "\n";
		(*m_ostr) << "jmpcnd\n";
	}

	// if block
	std::streampos before_if_block = m_ostr->tellp();
	ast->GetIfBlock()->accept(this, level+1);
	if(ast->GetElseBlock())
	{
		if(m_binary)
		{
			// skip to end of if statement if there's an else block
			m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH)); // push jump address
			m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_IP));
			skip_else_addr = m_ostr->tellp();
			m_ostr->write(reinterpret_cast<const char*>(&skipEndIf),
				vm_type_size<VMType::ADDR_IP, false>);
			m_ostr->put(static_cast<t_vm_byte>(OpCode::JMP));
		}
		else
		{
			(*m_ostr) << "push addr end_if_" << labelEndIf << "\n";
			(*m_ostr) << "jmp\n";
		}
	}
	std::streampos after_if_block = m_ostr->tellp();

	if(m_binary)
	{
		// go back and fill in missing number of bytes to skip
		skipEndCond = after_if_block - before_if_block;
		m_ostr->seekp(skip_addr);
		m_ostr->write(reinterpret_cast<const char*>(&skipEndCond),
			vm_type_size<VMType::ADDR_IP, false>);
		m_ostr->seekp(after_if_block);
	}
	else
	{
		(*m_ostr) << "end_cond_" << labelEndCond << ":" << std::endl;
	}

	// else block
	if(ast->GetElseBlock())
	{
		std::streampos before_else_block = m_ostr->tellp();
		ast->GetElseBlock()->accept(this, level+1);
		std::streampos after_else_block = m_ostr->tellp();

		if(m_binary)
		{
			// go back and fill in missing number of bytes to skip
			skipEndIf = after_else_block - before_else_block;
			m_ostr->seekp(skip_else_addr);
			m_ostr->write(reinterpret_cast<const char*>(&skipEndIf),
				vm_type_size<VMType::ADDR_IP, false>);
			m_ostr->seekp(after_else_block);
		}
		else
		{
			(*m_ostr) << "end_if_" << labelEndIf << ":" << std::endl;
		}
	}
}


void ASTAsm::visit(const ASTLoop* ast, [[maybe_unused]] std::size_t level)
{
	std::size_t labelLoop = m_glob_label++;

	std::ostringstream ostrLabel;
	ostrLabel << "loop_" << labelLoop;
	m_cur_loop.push_back(ostrLabel.str());

	std::streampos loop_begin = m_ostr->tellp();

	if(!m_binary)
	{
		(*m_ostr) << "begin_loop_" << labelLoop << ":" << std::endl;
	}

	ast->GetCondition()->accept(this, level+1); // condition

	t_vm_addr skip = 0;  // how many bytes to skip to jump to end of the block?
	std::streampos skip_addr = 0;

	if(m_binary)
	{
		m_ostr->put(static_cast<t_vm_byte>(OpCode::NOT));

		m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH)); // push jump address
		m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_IP));
		skip_addr = m_ostr->tellp();
		m_ostr->write(reinterpret_cast<const char*>(&skip),
			vm_type_size<VMType::ADDR_IP, false>);
		m_ostr->put(static_cast<t_vm_byte>(OpCode::JMPCND));
	}
	else
	{
		(*m_ostr) << "not\n";
		(*m_ostr) << "push addr end_loop_" << labelLoop << "\n";
		(*m_ostr) << "jmpcnd\n";
	}

	std::streampos before_block = m_ostr->tellp();
	ast->GetBlock()->accept(this, level+1); // block

	if(m_binary)
	{
		// loop back
		m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH)); // push jump address
		m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_IP));
		std::streampos after_block = m_ostr->tellp();
		skip = after_block - before_block;
		t_vm_addr skip_back = loop_begin - after_block;
		skip_back -= vm_type_size<VMType::ADDR_IP, true>; // include the next two writes
		m_ostr->write(reinterpret_cast<const char*>(&skip_back),
			vm_type_size<VMType::ADDR_IP, false>);
		m_ostr->put(static_cast<t_vm_byte>(OpCode::JMP));

		// go back and fill in missing number of bytes to skip
		after_block = m_ostr->tellp();
		skip = after_block - before_block;
		m_ostr->seekp(skip_addr);
		m_ostr->write(reinterpret_cast<const char*>(&skip),
			vm_type_size<VMType::ADDR_IP, false>);

		// fill in any saved, unset start-of-loop jump addresses (continues)
		while(true)
		{
			auto iter = m_loop_begin_comefroms.find(ostrLabel.str());
			if(iter == m_loop_begin_comefroms.end())
				break;

			std::streampos pos = iter->second;
			m_loop_begin_comefroms.erase(iter);

			t_vm_addr to_skip = loop_begin - pos;
			// already skipped over address and jmp instruction
			to_skip -= vm_type_size<VMType::ADDR_IP, true>;
			m_ostr->seekp(pos);
			m_ostr->write(reinterpret_cast<const char*>(&to_skip),
				vm_type_size<VMType::ADDR_IP, false>);
		}

		// fill in any saved, unset end-of-loop jump addresses (breaks)
		while(true)
		{
			auto iter = m_loop_end_comefroms.find(ostrLabel.str());
			if(iter == m_loop_end_comefroms.end())
				break;

			std::streampos pos = iter->second;
			m_loop_end_comefroms.erase(iter);

			t_vm_addr to_skip = after_block - pos;
			// already skipped over address and jmp instruction
			to_skip -= vm_type_size<VMType::ADDR_IP, true>;
			m_ostr->seekp(pos);
			m_ostr->write(reinterpret_cast<const char*>(&to_skip),
				vm_type_size<VMType::ADDR_IP, false>);
		}

		// go to end of stream
		m_ostr->seekp(after_block);
	}
	else
	{
		(*m_ostr) << "push addr begin_loop_" << labelLoop << "\n";
		(*m_ostr) << "jmp\n";

		(*m_ostr) << "end_loop_" << labelLoop << ":" << std::endl;
	}

	m_cur_loop.pop_back();
}


void ASTAsm::visit(const ASTFunc* ast, [[maybe_unused]] std::size_t level)
{
	if(m_cur_func != "")
		throw_err(ast, "Nested functions are not allowed.");

	// function name
	const std::string& func_name = ast->GetName();
	m_cur_func = func_name;

	// number of function arguments
	t_vm_int num_args = static_cast<t_vm_int>(ast->NumArgs());

	std::streampos jmp_end_streampos;
	t_vm_addr end_func_addr = 0;

	if(m_binary)
	{
		// jump to the end of the function to prevent accidental execution
		m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH)); // push jump address
		m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_IP));
		jmp_end_streampos = m_ostr->tellp();
		m_ostr->write(reinterpret_cast<const char*>(&end_func_addr),
			vm_type_size<VMType::ADDR_IP, false>);
		m_ostr->put(static_cast<t_vm_byte>(OpCode::JMP));
	}
	else
	{
		(*m_ostr) << "jmp " << func_name << "_end\n";
		(*m_ostr) << func_name << ":\n";
	}


	// function arguments
	if(m_binary && ast->GetArgs())
	{
		// skip saved ebp and return address on stack frame
		//t_vm_addr addr_bp = 2 * vm_type_size<VMType::ADDR_IP, true>;

		for(std::size_t i=0; i<ast->GetArgs()->NumChildren(); ++i)
		{
			auto ident = std::dynamic_pointer_cast<ASTToken<std::string>>(ast->GetArgs()->GetChild(i));
			const std::string& argname = ident->GetLexerValue();
			std::string varname = m_cur_func + "/" + argname;

			constexpr VMType argty = VMType::UNKNOWN;
			//std::cout << "arg: " << varname << ", addr: " << addr_bp << std::endl;
			//m_symtab.AddSymbol(varname, addr_bp, VMType::ADDR_BP, argty);
			//addr_bp += vm_type_size<argty, true>;

			//std::cout << "arg: " << varname << ", addr: " << i+2 << std::endl;
			m_symtab.AddSymbol(varname, i+2, VMType::ADDR_BP_ARG, argty);
		}
	}


	std::streampos before_block = m_ostr->tellp();

	if(m_binary)
	{
		// add function to symbol table
		m_symtab.AddSymbol(func_name, before_block, VMType::ADDR_MEM, VMType::UNKNOWN, true, num_args);
		//std::cout << "function " << func_name << " at address " << before_block << std::endl;
	}

	ast->GetBlock()->accept(this, level+1); // block


	if(m_binary)
	{
		std::streampos ret_streampos = m_ostr->tellp();

		// push number of arguments and return
		m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH));
		m_ostr->put(static_cast<t_vm_byte>(VMType::INT));
		m_ostr->write(reinterpret_cast<const char*>(&num_args),
			vm_type_size<VMType::INT, false>);
		m_ostr->put(static_cast<t_vm_byte>(OpCode::RET));

		// fill in end-of-function jump address
		std::streampos end_func_streampos = m_ostr->tellp();
		end_func_addr = end_func_streampos - before_block;
		m_ostr->seekp(jmp_end_streampos);
		m_ostr->write(reinterpret_cast<const char*>(&end_func_addr),
			vm_type_size<VMType::ADDR_IP, false>);

		// fill in any saved, unset end-of-function jump addresses
		for(std::streampos pos : m_endfunc_comefroms)
		{
			t_vm_addr to_skip = ret_streampos - pos;
			// already skipped over address and jmp instruction
			to_skip -= vm_type_size<VMType::ADDR_IP, true>;
			m_ostr->seekp(pos);
			m_ostr->write(reinterpret_cast<const char*>(&to_skip),
				vm_type_size<VMType::ADDR_IP, false>);
		}
		m_endfunc_comefroms.clear();
		m_ostr->seekp(end_func_streampos);
	}
	else
	{
		(*m_ostr) << func_name << "_ret:" << std::endl;
		(*m_ostr) << "ret " << num_args << "\n";
		(*m_ostr) << func_name << "_end:" << std::endl;
	}

	m_cur_func = "";
	m_cur_loop.clear();
}


void ASTAsm::visit(const ASTFuncCall* ast, [[maybe_unused]] std::size_t level)
{
	const std::string& func_name = ast->GetName();
	t_vm_int num_args = static_cast<t_vm_int>(ast->NumArgs());
	bool is_external_func = (m_always_call_ext || m_ext_funcs.contains(func_name));

	// push the function arguments
	if(ast->GetArgs())
		ast->GetArgs()->accept(this, level+1);

	if(m_binary)
	{
		// call external function
		if(is_external_func)
		{
			// push external function name
			m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH));
			// write type descriptor byte
			m_ostr->put(static_cast<t_vm_byte>(VMType::STR));
			// write function name
			t_vm_addr len = static_cast<t_vm_addr>(func_name.length());
			m_ostr->write(reinterpret_cast<const char*>(&len),
				vm_type_size<VMType::ADDR_MEM, false>);
			// write string data
			m_ostr->write(func_name.data(), len);

			// TODO: check number of arguments

			m_ostr->put(static_cast<t_vm_byte>(OpCode::EXTCALL));
		}

		// call internal function
		else
		{
			// get function address and push it
			const SymInfo *sym = m_symtab.GetSymbol(func_name);
			t_vm_addr func_addr = 0;
			if(sym)
			{
				// function address already known
				func_addr = sym->addr;

				if(num_args != sym->num_args)
				{
					std::ostringstream msg;
					msg << "Function \"" << func_name << "\" takes " << sym->num_args
						<< " arguments, but " << num_args << " were given.";
					throw_err(ast, msg.str());
				}
			}

			m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH));
	#if AST_ABS_FUNC_ADDR != 0
			// push absolute function address
			m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_MEM));
			std::streampos addr_pos = m_ostr->tellp();
			m_ostr->write(reinterpret_cast<const char*>(&func_addr),
				vm_type_size<VMType::ADDR_MEM, false>);
	#else
			// push relative function address
			m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_IP));
			// already skipped over address and jmp instruction
			std::streampos addr_pos = m_ostr->tellp();
			t_vm_addr to_skip = static_cast<t_vm_addr>(func_addr - addr_pos);
			to_skip -= vm_type_size<VMType::ADDR_IP, true>;
			m_ostr->write(reinterpret_cast<const char*>(&to_skip),
				vm_type_size<VMType::ADDR_IP, false>);
	#endif
			m_ostr->put(static_cast<t_vm_byte>(OpCode::CALL));

			if(!sym)
			{
				// function address not yet known
				m_func_comefroms.emplace_back(
					std::make_tuple(func_name, addr_pos, num_args, ast));
			}
		}
	}
	else
	{
		if(is_external_func)
		{
			// call external function
			(*m_ostr) << "extcall " << func_name << std::endl;
		}
		else
		{
			// call internal function
			(*m_ostr) << "call " << func_name << std::endl;
		}
	}
}


void ASTAsm::visit(const ASTJump* ast, [[maybe_unused]] std::size_t level)
{
	if(ast->GetJumpType() == ASTJump::JumpType::RETURN)
	{
		if(ast->GetExpr())
			ast->GetExpr()->accept(this, level+1);

		if(m_cur_func == "")
			throw_err(ast, "Tried to return outside any function.");

		if(m_binary)
		{
			// jump to the end of the function
			m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH)); // push jump address
			m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_IP));
			m_endfunc_comefroms.push_back(m_ostr->tellp());
			t_vm_addr dummy_addr = 0;
			m_ostr->write(reinterpret_cast<const char*>(&dummy_addr),
				vm_type_size<VMType::ADDR_IP, false>);
			m_ostr->put(static_cast<t_vm_byte>(OpCode::JMP));
		}
		else
		{
			(*m_ostr) << "jmp " << m_cur_func << "_end" << std::endl;
		}
	}
	else if(ast->GetJumpType() == ASTJump::JumpType::BREAK
		|| ast->GetJumpType() == ASTJump::JumpType::CONTINUE)
	{
		if(!m_cur_loop.size())
			throw_err(ast, "Tried to use break/continue outside loop.");

		t_int loop_depth = 0; // how many loop levels to break/continue?

		if(ast->GetExpr())
		{
			auto int_val = std::dynamic_pointer_cast<ASTToken<t_int>>(ast->GetExpr());
			auto real_val = std::dynamic_pointer_cast<ASTToken<t_real>>(ast->GetExpr());

			if(int_val)
				loop_depth = int_val->GetLexerValue();
			else if(real_val)
				loop_depth = static_cast<t_int>(std::round(real_val->GetLexerValue()));
		}

		// reduce to maximum loop depth
		if(static_cast<std::size_t>(loop_depth) >= m_cur_loop.size() || loop_depth < 0)
			loop_depth = static_cast<t_int>(m_cur_loop.size()-1);

		const std::string& cur_loop = *(m_cur_loop.rbegin() + loop_depth);

		if(m_binary)
		{
			// jump to the beginning (continue) or end (break) of the loop
			m_ostr->put(static_cast<t_vm_byte>(OpCode::PUSH)); // push jump address
			m_ostr->put(static_cast<t_vm_byte>(VMType::ADDR_IP));
			if(ast->GetJumpType() == ASTJump::JumpType::BREAK)
				m_loop_end_comefroms.insert(std::make_pair(cur_loop, m_ostr->tellp()));
			else if(ast->GetJumpType() == ASTJump::JumpType::CONTINUE)
				m_loop_begin_comefroms.insert(std::make_pair(cur_loop, m_ostr->tellp()));
			t_vm_addr dummy_addr = 0;
			m_ostr->write(reinterpret_cast<const char*>(&dummy_addr),
				vm_type_size<VMType::ADDR_IP, false>);
			m_ostr->put(static_cast<t_vm_byte>(OpCode::JMP));
		}
		else
		{
			if(ast->GetJumpType() == ASTJump::JumpType::BREAK)
				(*m_ostr) << "jmp end_" << cur_loop << std::endl;
			else if(ast->GetJumpType() == ASTJump::JumpType::CONTINUE)
				(*m_ostr) << "jmp begin_" << cur_loop << std::endl;
		}
	}
}


void ASTAsm::visit(const ASTDeclare* ast, [[maybe_unused]] std::size_t level)
{
	std::size_t num_idents = ast->NumIdents();

	// external function declarations
	if(ast->IsFunc() && ast->IsExternal())
	{
		for(std::size_t idx=0; idx<num_idents; ++idx)
		{
			const std::string* func_name = ast->GetIdent(idx);
			if(func_name)
				m_ext_funcs.insert(*func_name);
		}
	}
}


/**
 * fill in function addresses for calls
 */
void ASTAsm::PatchFunctionAddresses()
{
	for(const auto& [func_name, pos, num_args, call_ast] : m_func_comefroms)
	{
		const SymInfo *sym = m_symtab.GetSymbol(func_name);
		if(!sym)
		{
			throw_err(call_ast,
				"Tried to call unknown function \"" + func_name + "\".");
		}

		if(num_args != sym->num_args)
		{
			std::ostringstream msg;
			msg << "Function \"" << func_name << "\" takes " << sym->num_args
				<< " arguments, but " << num_args << " were given.";
			throw_err(call_ast, msg.str());
		}

		/*std::cout << "Patching function \"" << func_name << "\""
			<< " at address " << sym->addr
			<< " to stream position " << pos
			<< "." << std::endl;*/

		m_ostr->seekp(pos);

#if AST_ABS_FUNC_ADDR != 0
		// write absolute function address
		m_ostr->write(reinterpret_cast<const char*>(&sym->addr),
			vm_type_size<VMType::ADDR_IP, false>);
#else
		// write relative function address
		t_vm_addr to_skip = static_cast<t_vm_addr>(sym->addr - pos);
		// already skipped over address and jmp instruction
		to_skip -= vm_type_size<VMType::ADDR_IP, true>;
		m_ostr->write(reinterpret_cast<const char*>(&to_skip),
			vm_type_size<VMType::ADDR_IP, false>);
#endif
	}

	// seek to end of stream
	m_ostr->seekp(0, std::ios_base::end);
}


void ASTAsm::FinishCodegen()
{
	// add a final halt instruction
	m_ostr->put(static_cast<t_vm_byte>(OpCode::HALT));
}
