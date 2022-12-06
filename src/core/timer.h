/**
 * helper functions
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date july-2022
 * @license: see 'LICENSE' file
 */

#ifndef __LALR1_TIME_HELPERS__
#define __LALR1_TIME_HELPERS__


#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

#if __has_include(<time.h>)
	// TODO: find a better way to test if localtime_r exists
	#define __LOCTIME_FUNC 0
	//#pragma message("Using localtime_r.")
#elif __has_include(<boost/date_time.hpp>)
	#include <boost/date_time.hpp>
	#define __LOCTIME_FUNC 1
	//#pragma message("Using boost localtime.")
#else
	#define __LOCTIME_FUNC 2
	#pragma message("Using unsafe localtime.")
#endif


namespace lalr1 {

using t_clock = std::chrono::steady_clock;
using t_timepoint = std::chrono::time_point<t_clock>;


/**
 * get time since start_time
 */
template<class t_real, class t_timepoint>
std::tuple<t_real, std::string>
get_elapsed_time(const t_timepoint& start_time)
{
	using t_duration_ms = std::chrono::duration<t_real, std::ratio<1, 1000>>;
	using t_clock = typename t_timepoint::clock;
	t_duration_ms ms = t_clock::now() - start_time;
	t_real run_time = ms.count();

	std::string time_unit = "ms";
	if(run_time >= t_real(1000.))
	{
		using t_duration_s = std::chrono::duration<t_real, std::ratio<1, 1>>;
		t_duration_s s = ms;
		run_time = s.count();
		time_unit = "s";

		if(run_time >= t_real(60.))
		{
			using t_duration_min = std::chrono::duration<t_real, std::ratio<60, 1>>;
			t_duration_min min = s;
			run_time = min.count();
			time_unit = "min";
		}
	}

	return std::make_tuple(run_time, time_unit);
}


static inline std::tm* safe_localtime(const std::time_t* tm, std::tm* loctime)
{
#if __LOCTIME_FUNC == 0
	loctime = localtime_r(tm, loctime);
#elif __LOCTIME_FUNC == 1
	// normally also just calls localtime_r
	loctime = boost::date_time::c_time::localtime(tm, loctime);
#elif __LOCTIME_FUNC == 2
	// not thread-safe!
	loctime = std::localtime(tm);
#endif

	return loctime;
}


template<class t_clock = std::chrono::system_clock>
std::string get_timestamp()
{
	std::time_t now{t_clock::to_time_t(t_clock::now())};
	std::tm loctime{};

	std::ostringstream ostr;
	ostr << std::put_time(safe_localtime(&now, &loctime), "%d/%m/%Y %Hh%M");
	return ostr.str();
}

} // namespace lalr1

#endif
