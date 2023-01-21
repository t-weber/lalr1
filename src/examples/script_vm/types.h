/**
 * asm generator and vm opcodes
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14-jun-2022
 * @license see 'LICENSE' file
 */

#ifndef __LR1_VM_TYPES_H__
#define __LR1_VM_TYPES_H__

#include <cstdint>
#include <string>
#include "script/lval.h"


using t_vm_int = ::t_int;
using t_vm_real = ::t_real;
//using t_vm_addr = std::int16_t;
using t_vm_addr = std::int32_t;
using t_vm_byte = std::uint8_t;
using t_vm_bool = t_vm_byte;
using t_vm_str = std::string;

// maximum size to reserve for static variables
constexpr const t_vm_addr g_vm_longest_size = 64;



enum class VMType : t_vm_byte
{
	UNKNOWN     = 0x00,
	REAL        = 0x01,
	INT         = 0x02,
	BOOLEAN     = 0x03,
	STR         = 0x04,

	ADDR_MEM    = 0b00001000,  // address refering to absolute memory locations
	ADDR_IP     = 0b00001001,  // address relative to the instruction pointer
	ADDR_SP     = 0b00001010,  // address relative to the stack pointer
	ADDR_BP     = 0b00001011,  // address relative to a local base pointer
	ADDR_GBP    = 0b00001100,  // address relative to the global base pointer
	ADDR_HP     = 0b00001101,  // address relative to the heap pointer

	ADDR_BP_ARG = 0b00011011,  // number of arguments from base pointer
};



/**
 * get a string representation of a base register name
 */
template<class t_str = const char*>
constexpr t_str get_vm_base_reg(VMType ty)
{
	switch(ty)
	{
		case VMType::UNKNOWN:     return "unknown";
		case VMType::ADDR_MEM:    return "absolute";
		case VMType::ADDR_IP:     return "ip";
		case VMType::ADDR_SP:     return "sp";
		case VMType::ADDR_BP:     return "bp";
		case VMType::ADDR_GBP:    return "gbp";
		case VMType::ADDR_HP:     return "hp";
		case VMType::ADDR_BP_ARG: return "index/bp";
		default:                  return "<unknown>";
	}
}



/**
 * get a string representation of a type name
 * (run-time version)
 */
template<class t_str = const char*>
constexpr t_str get_vm_type_name(VMType ty)
{
	switch(ty)
	{
		case VMType::UNKNOWN:     return "unknown";
		case VMType::REAL:        return "real";
		case VMType::INT:         return "integer";
		case VMType::BOOLEAN:     return "boolean";
		case VMType::STR:         return "string";
		case VMType::ADDR_MEM:    return "absolute address";
		case VMType::ADDR_IP:     return "address relative to ip";
		case VMType::ADDR_SP:     return "address relative to sp";
		case VMType::ADDR_BP:     return "address relative to bp";
		case VMType::ADDR_GBP:    return "address relative to gbp";
		case VMType::ADDR_HP:     return "address relative to hp";
		case VMType::ADDR_BP_ARG: return "argument index relative bp";
		default:                  return "<unknown>";
	}
}



/**
 * get VMType sizes (including data type and, optionally, descriptor byte)
 * (static version)
 */
template<VMType ty, bool with_descr = false> constexpr t_vm_addr vm_type_size
	= g_vm_longest_size + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::UNKNOWN, with_descr>
	= g_vm_longest_size + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::REAL, with_descr>
	= sizeof(t_vm_real) + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::INT, with_descr>
	= sizeof(t_vm_int) + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::STR, with_descr>
	= g_vm_longest_size + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::BOOLEAN, with_descr>
	= sizeof(t_vm_bool) + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::ADDR_MEM, with_descr>
	= sizeof(t_vm_addr) + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::ADDR_IP, with_descr>
	= sizeof(t_vm_addr) + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::ADDR_SP, with_descr>
	= sizeof(t_vm_addr) + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::ADDR_BP, with_descr>
	= sizeof(t_vm_addr) + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::ADDR_GBP, with_descr>
	= sizeof(t_vm_addr) + (with_descr ? sizeof(t_vm_byte) : 0);
template<bool with_descr> constexpr inline t_vm_addr vm_type_size<VMType::ADDR_HP, with_descr>
	= sizeof(t_vm_addr) + (with_descr ? sizeof(t_vm_byte) : 0);



/**
 * get VMType sizes (including data type and, optionally, descriptor byte)
 * (run-time version)
 */
static inline t_vm_addr get_vm_type_size(VMType ty, bool with_descr = false)
{
	t_vm_addr size = 0;

	switch(ty)
	{
		case VMType::UNKNOWN:
			size = g_vm_longest_size;
			break;
		case VMType::REAL:
			size = sizeof(t_vm_real);
			break;
		case VMType::INT:
			size = sizeof(t_vm_int);
			break;
		case VMType::STR:
			size = g_vm_longest_size;
			break;
		case VMType::BOOLEAN:
			size = sizeof(t_vm_bool);
			break;
		case VMType::ADDR_MEM:
		case VMType::ADDR_IP:
		case VMType::ADDR_SP:
		case VMType::ADDR_BP:
		case VMType::ADDR_GBP:
		case VMType::ADDR_HP:
			size = sizeof(t_vm_addr);
			break;
		default:
			size = g_vm_longest_size;
			break;
	}

	if(with_descr)
		size += sizeof(t_vm_byte);

	return size;
}



/**
 * get derived data type for casting
 */
static inline VMType derive_data_type(VMType ty1, VMType ty2)
{
	if(ty1 == ty2)
		return ty1;
	else if(ty1 == VMType::STR || ty2 == VMType::STR)
		return VMType::STR;
	else if(ty1 == VMType::INT && ty2 == VMType::REAL)
		return VMType::REAL;
	else if(ty1 == VMType::REAL && ty2 == VMType::INT)
		return VMType::REAL;

	return VMType::UNKNOWN;
}


#endif
