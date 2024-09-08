/**
 * lalr(1) parser table
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_TABLE_H__
#define __LALR1_TABLE_H__


#include <vector>
#include <unordered_map>
#include <optional>
#include <iostream>
#include <iomanip>
#include <cmath>

#include "types.h"


namespace lalr1 {

template<
	class T = std::size_t,
	template<class...> class t_cont = std::vector>
class Table
{
public:
	using value_type = T;
	using container_type = t_cont<T>;


	Table() = default;
	~Table() = default;


	Table(const t_cont<t_cont<T>>& cont,
		T errorval = 0xffffffff, T acceptval = 0xfffffffe, T fillval = 0xffffffff,
		std::optional<std::size_t> ROWS = std::nullopt,
		std::optional<std::size_t> COLS = std::nullopt)
		: m_errorval{errorval}, m_acceptval{acceptval}, m_fillval{fillval}
	{
		m_rowsize = ROWS ? *ROWS : cont.size();
		m_colsize = COLS ? *COLS : 0;

		if(!COLS)
		{
			for(const auto& controw : cont)
				m_colsize = std::max(controw.size(), m_colsize);
		}
		m_data.resize(m_rowsize * m_colsize, fillval);

		for(std::size_t row=0; row<m_rowsize; ++row)
		{
			if(row >= cont.size())
				break;

			const auto& controw = cont[row];
			for(std::size_t col=0; col<m_colsize; ++col)
				this->operator()(row, col) = (col < controw.size() ? controw[col] : errorval);
		}
	}


	Table(std::size_t ROWS, std::size_t COLS) : m_data(ROWS*COLS), m_rowsize{ROWS}, m_colsize{COLS}
	{}

	Table(std::size_t ROWS, std::size_t COLS,
		T errorval = 0xffffffff, T acceptval = 0xfffffffe, T fillval = 0xffffffff,
		const std::initializer_list<T>& lst = {})
		: m_data{lst}, m_rowsize{ROWS}, m_colsize{COLS},
			m_errorval{errorval}, m_acceptval{acceptval}, m_fillval{fillval}
	{}


	std::size_t size1() const { return m_rowsize; }
	std::size_t size2() const { return m_colsize; }


	const T& operator()(std::size_t row, std::size_t col) const
	{
		return m_data[row*m_colsize + col];
	}

	T& operator()(std::size_t row, std::size_t col)
	{
		return m_data[row*m_colsize + col];
	}


	/**
	 * merge the values of another table into this one
	 */
	void MergeTable(const Table<T, t_cont>& tab, bool show_conflict = true)
	{
		std::size_t rows = std::min(this->size1(), tab.size1());
		std::size_t cols = std::min(this->size2(), tab.size2());

		for(std::size_t row=0; row<rows; ++row)
		{
			for(std::size_t col=0; col<cols; ++col)
			{
				const value_type& val = tab(row, col);
				if(val != m_errorval && val != m_fillval)
				{
					value_type& oldval = (*this)(row, col);

					if(show_conflict && oldval != m_errorval && oldval != m_fillval)
					{
						std::cerr << "Warning: row " << row
							<< " and column " << col
							<< " are already occupied."
							<< std::endl;
					}

					oldval = val;
				}
			}
		}
	}


	/**
	 * print table
	 */
	friend std::ostream& operator<<(std::ostream& ostr, const Table<T, t_cont>& tab)
	{
		const int width = 7;
		for(std::size_t row=0; row<tab.size1(); ++row)
		{
			for(std::size_t col=0; col<tab.size2(); ++col)
			{
				const value_type& entry = tab(row, col);
				if(entry == tab.m_errorval)
					ostr << std::setw(width) << std::left << "err";
				else if(entry == tab.m_acceptval)
					ostr << std::setw(width) << std::left << "acc";
				else
					ostr << std::setw(width) << std::left << entry;
			}
			ostr << "\n";
		}
		return ostr;
	}


	const T& GetErrorVal() const { return m_errorval; }
	const T& GetAcceptVal() const { return m_acceptval; }
	const T& GetFillVal() const { return m_fillval; }


private:
	container_type m_data{};
	std::size_t m_rowsize{}, m_colsize{};

	T m_errorval{};
	T m_acceptval{};
	T m_fillval{};
};

} // namespace lalr1

#endif
