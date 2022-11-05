/**
 * basic types
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 8-oct-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_TYPES_H__
#define __LALR1_TYPES_H__

#include <cstddef>
#include <type_traits>


using t_index       = std::size_t;
using t_hash        = std::size_t;

using t_symbol_id   = std::size_t;
using t_state_id    = std::size_t;
using t_semantic_id = std::size_t;


/**
 * get the rust equivalent name of the given type
 */
template<typename T>
constexpr const char* get_rs_typename()
{
	if constexpr(std::is_integral_v<T>)
	{
		if constexpr(std::is_signed_v<T>)
		{
			if constexpr(sizeof(T) == sizeof(std::size_t))
				return "isize";
			else if constexpr(sizeof(T) == 1)
				return "i8";
			else if constexpr(sizeof(T) == 2)
				return "i16";
			else if constexpr(sizeof(T) == 4)
				return "i32";
			else if constexpr(sizeof(T) == 8)
				return "i64";
			else if constexpr(sizeof(T) == 16)
				return "i128";
			else
				return "<unknown>";
		}
		else
		{
			if constexpr(sizeof(T) == sizeof(std::size_t))
				return "usize";
			else if constexpr(sizeof(T) == 1)
				return "u8";
			else if constexpr(sizeof(T) == 2)
				return "u16";
			else if constexpr(sizeof(T) == 4)
				return "u32";
			else if constexpr(sizeof(T) == 8)
				return "u64";
			else if constexpr(sizeof(T) == 16)
				return "u128";
			else
				return "<unknown>";
		}
	}
	else if constexpr(std::is_floating_point_v<T>)
	{
		if constexpr(sizeof(T) == 4)
			return "f32";
		else if constexpr(sizeof(T) == 8)
			return "f64";
		else if constexpr(sizeof(T) == 16)
			return "f128";
		else
			return "<unknown>";
	}
	else
	{
		return "<unknown>";
	}
}


#endif
