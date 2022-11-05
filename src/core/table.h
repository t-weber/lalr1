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
	 * export table to C++ code
	 */
	void SaveCXX(std::ostream& ostr, const std::string& var,
		const std::string& row_label = "", const std::string& col_label = "") const
	{
		ostr << "const t_table " << var << "{";
		ostr << size1();
		if(row_label.size())
			ostr << " /*" << row_label << "*/";
		ostr << ", " << size2();
		if(col_label.size())
			ostr << " /*" << col_label << "*/";
		ostr << ", " << "err, acc, ";
		if(m_fillval == m_errorval)
			ostr << "err, ";
		else if(m_fillval == m_acceptval)
			ostr << "acc, ";
		else
			ostr << m_fillval << ", ";
		ostr << "\n";

		ostr << "{\n";
		for(std::size_t row=0; row<size1(); ++row)
		{
			ostr << "\t";
			for(std::size_t col=0; col<size2(); ++col)
			{
				const value_type& entry = operator()(row, col);

				if(entry == m_errorval)
					ostr << "err, ";
				else if(entry == m_acceptval)
					ostr << "acc, ";
				else
					ostr << entry << ", ";
			}
			ostr << "// " << row_label << " " << row << "\n";
		}
		ostr << "}";
		ostr << "};\n\n";
	}


	/**
	 * export table to Java code
	 */
	void SaveJava(std::ostream& ostr, const std::string& var,
		const std::string& row_label = "", const std::string& col_label = "",
		const std::string& acc_level = "public", std::size_t indent = 0) const
	{
		auto do_indent = [&ostr, indent]()
		{
			for(std::size_t i=0; i<indent; ++i)
				ostr << "\t";
		};

		do_indent();
		ostr << acc_level << " final int[";
		//ostr << " " << size1();
		if(row_label != "")
			ostr << " /*" << row_label << "*/ ";
		ostr << "][";
		//ostr << " " << size2();
		if(col_label != "")
			ostr << " /*" << col_label << "*/ ";
		ostr << "] " << var << " =\n";

		do_indent();
		ostr << "{\n";
		for(std::size_t row=0; row<size1(); ++row)
		{
			do_indent();
			ostr << "\t{ ";
			for(std::size_t col=0; col<size2(); ++col)
			{
				const value_type& entry = operator()(row, col);

				if(entry == m_errorval)
					ostr << "err, ";
				else if(entry == m_acceptval)
					ostr << "acc, ";
				else
					ostr << entry << ", ";
			}
			ostr << "}, // " << row_label << " " << row << "\n";
		}

		do_indent();
		ostr << "};\n\n";
	}


	/**
	 * export table to json
	 */
	void SaveJSON(std::ostream& ostr, const std::string& var,
		const std::string& row_label = "", const std::string& col_label = "",
		const std::unordered_map<T, int>* value_map = nullptr) const
	{
		ostr << "\"" << var << "\" : {\n";
		ostr << "\t\"rows\" : " << size1() << ",\n";
		ostr << "\t\"cols\" : " << size2() << ",\n";
		ostr << "\t\"row_label\" : \"" << row_label << "\",\n";
		ostr << "\t\"col_label\" : \"" << col_label << "\",\n";

		ostr << "\t\"elems\" : [\n";
		for(std::size_t row=0; row<size1(); ++row)
		{
			ostr << "\t\t[ ";
			for(std::size_t col=0; col<size2(); ++col)
			{
				value_type entry = operator()(row, col);
				/*if(entry == m_errorval || entry == m_acceptval)
					ostr << "0x" << std::hex << entry << std::dec;
				else
					ostr << entry;*/

				bool has_value = false;
				if(value_map)
				{
					if(auto iter_val = value_map->find(entry); iter_val != value_map->end())
					{
						// write mapped value
						ostr << iter_val->second;
						has_value = true;
					}
				}

				// write unmapped value otherwise
				if(!has_value)
					ostr << entry;

				if(col < size2()-1)
					ostr << ",";
				ostr << " ";
			}
			ostr << "]";
			if(row < size1()-1)
				ostr << ",";
			ostr << "\n";
		}
		ostr << "\t]\n";

		ostr << "}";
	}


	/**
	 * export table to rust
	 */
	void SaveRS(std::ostream& ostr, const std::string& var,
		const std::string& row_label = "", const std::string& col_label = "") const
	{
		std::string ty = get_rs_typename<value_type>();

		ostr << "pub const " << var;
		ostr << " : [[" << ty << "; " << size2();
		if(col_label != "")
			ostr << " /* " << col_label << " */";
		ostr << "]; " << size1();
		if(row_label != "")
			ostr << " /* " << row_label << " */";
		ostr << "]";
		ostr << " =\n[\n";

		for(std::size_t row=0; row<size1(); ++row)
		{
			ostr << "\t[ ";
			for(std::size_t col=0; col<size2(); ++col)
			{
				value_type entry = operator()(row, col);
				if(entry == m_errorval)
					ostr << "err";
				else if(entry == m_acceptval)
					ostr << "acc";
				else
					ostr << entry;

				if(col < size2()-1)
					ostr << ",";
				ostr << " ";
			}
			ostr << "]";
			if(row < size1()-1)
				ostr << ",";

			if(row_label != "")
				ostr << " // " << row_label << " " << row;

			ostr << "\n";
		}
		ostr << "];\n";
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


private:
	container_type m_data{};
	std::size_t m_rowsize{}, m_colsize{};

	T m_errorval{};
	T m_acceptval{};
	T m_fillval{};
};


#endif
