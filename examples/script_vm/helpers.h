/**
 * helper functions for vm
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 11-jun-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_0ACVM_HELPERS_H__
#define __LALR1_0ACVM_HELPERS_H__


#include <type_traits>
#include <cmath>


template<class t_val>
t_val pow(t_val val1, t_val val2)
{
	if constexpr(std::is_floating_point_v<t_val>)
	{
		return std::pow(val1, val2);
	}
	else if constexpr(std::is_integral_v<t_val>)
	{
		if(val2 == 0)
			return 1;
		else if(val2 < 0)
			return 0;

		t_val result = val1;
		for(t_val i=1; i<val2; ++i)
			result *= val1;
		return result;
	}
}


#endif
