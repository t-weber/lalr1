/**
 * value type for lexer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LR1_LVAL_H__
#define __LR1_LVAL_H__

#include <variant>
#include <optional>
#include <string>
#include <cstdint>


//using t_real = float;
//using t_int = std::int32_t;
using t_real = double;
using t_int = std::int64_t;

using t_lval = std::optional<std::variant<t_real, t_int, std::string>>;


#endif
