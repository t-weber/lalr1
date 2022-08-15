/**
 * lalr(1) parser table
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14-jun-2020
 * @license see 'LICENSE' file
 */

#ifndef __LALR1_TABLE_H__
#define __LALR1_TABLE_H__


#include <optional>
#include <iostream>
#include <iomanip>


template<
	class T = std::size_t, template<class...>
		class t_cont = std::vector>
class Table
{
public:
	using value_type = T;
	using container_type = t_cont<T>;


	Table() = default;
	~Table() = default;


	Table(const t_cont<t_cont<T>>& cont,
		  T errorval=0xffffffff, T acceptval=0xfffffffe,
		  std::optional<std::size_t> ROWS=std::nullopt,
	   std::optional<std::size_t> COLS=std::nullopt)
		: m_data{}, m_rowsize{}, m_colsize{},
			m_errorval{errorval}, m_acceptval{acceptval}
	{
		m_rowsize = ROWS ? *ROWS : cont.size();
		m_colsize = COLS ? *COLS : 0;

		if(!COLS)
		{
			for(const auto& controw : cont)
				m_colsize = std::max(controw.size(), m_colsize);
		}
		m_data.resize(m_rowsize * m_colsize, errorval);

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
		T errorval=0xffffffff, T acceptval=0xfffffffe,
		const std::initializer_list<T>& lst={})
		: m_data{lst}, m_rowsize{ROWS}, m_colsize{COLS}, m_errorval{errorval}, m_acceptval{acceptval}
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
	 * export table to C++ code
	 */
	void SaveCXXDefinition(std::ostream& ostr, const std::string& var) const
	{
		ostr << "const Table<std::size_t, std::vector> " << var << "{"
			<< size1() << ", " << size2() << ", "
			<< "err, acc, ";

		ostr << "{\n";
		for(std::size_t row=0; row<size1(); ++row)
		{
			ostr << "\t";
			for(std::size_t col=0; col<size2(); ++col)
			{
				T entry = operator()(row, col);

				if(entry == m_errorval)
					ostr << "err, ";
				else if(entry == m_acceptval)
					ostr << "acc, ";
				else
					ostr << entry << ", ";
			}
			ostr << "\n";
		}
		ostr << "}";
		ostr << "};\n\n";
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
				T entry = tab(row, col);
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


private:
	container_type m_data{};
	std::size_t m_rowsize{}, m_colsize{};

	T m_errorval{0};
	T m_acceptval{0};
};


#endif
