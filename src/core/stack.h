/**
 * stack for symbols and states
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date sep-2022
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_STACK_H__
#define __LALR1_STACK_H__


#include <stack>

#ifndef LALR1_STACK_CONTAINER
	#include <deque>
	#define LALR1_STACK_CONTAINER std::deque

	//#include <vector>
	//#define LALR1_STACK_CONTAINER std::vector
#endif


namespace lalr1 {

/**
 * stack class for parse states and symbols
 */
template<class t_elem, class t_cont = LALR1_STACK_CONTAINER<t_elem>>
class ParseStack : public std::stack<t_elem, t_cont>
{
protected:
	// the underlying container
	using std::stack<t_elem, t_cont>::c;


public:
	using value_type = typename std::stack<t_elem, t_cont>::value_type;
	using iterator = typename t_cont::iterator;
	using const_iterator = typename t_cont::const_iterator;
	using reverse_iterator = typename t_cont::reverse_iterator;
	using const_reverse_iterator = typename t_cont::const_reverse_iterator;


public:
	iterator begin() { return c.begin(); }
	iterator end() { return c.end(); }
	const_iterator begin() const { return c.begin(); }
	const_iterator end() const { return c.end(); }

	reverse_iterator rbegin() { return c.rbegin(); }
	reverse_iterator rend() { return c.rend(); }
	const_reverse_iterator rbegin() const { return c.rbegin(); }
	const_reverse_iterator rend() const { return c.rend(); }

	/**
	 * get the top N elements on stack
	 */
	template<template<class...> class t_cont_ret>
	t_cont_ret<t_elem> topN(std::size_t N) const
	{
		t_cont_ret<t_elem> cont;

		std::size_t cnt = 0;
		for(auto iter = c.rbegin(); iter != c.rend() && cnt < N; std::advance(iter, 1))
		{
			//cont.push_back(*iter);
			cont.push_front(*iter);
			++cnt;
		}

		//std::reverse(cont.begin(), cont.end());
		return cont;
	}
};

} // namespace lalr1

#endif
