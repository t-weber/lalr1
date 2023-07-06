/**
 * memory dump and visualisation functions
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 9-july-2022
 * @license see 'LICENSE' file
 */

#include "vm.h"

#include <iostream>
#include <sstream>
#include <cstring>


#ifdef USE_BOOST_GIL

// see: https://github.com/boostorg/gil/tree/develop/example
#include <boost/gil/image.hpp>
#include <boost/gil/extension/io/png/write.hpp>


/**
 * visualises vm memory utilisation
 */
void VM::DrawMemoryImage()
{
	//using t_img = boost::gil::gray8_image_t;
	using t_img = boost::gil::rgb8_image_t;
	using t_view = typename t_img::view_t;
	using t_coord = typename t_view::coord_t;
	using t_pixel = typename boost::gil::channel_type<t_img>::type;
	using t_format = typename boost::gil::png_tag;

	std::size_t length = static_cast<std::size_t>(
		std::ceil(std::sqrt(static_cast<t_real>(m_memsize * 8))));

	int pixel_scale = 4;
	length *= pixel_scale;

	t_img img(length, length);
	t_view view = boost::gil::view(img);

	for(t_coord y = 0; y < view.height(); ++y)
	{
		t_coord x = 0;
		for(auto iter = view.row_begin(y); iter != view.row_end(y); ++iter)
		{
			std::uint64_t mem_pos = y/pixel_scale*view.width()/pixel_scale + x/pixel_scale;
			std::uint64_t mem_byte = mem_pos / 8;
			std::uint8_t mem_bit = mem_pos % 8;

			t_pixel pixel[] { 0x00, 0x00, 0x00 };
			if(mem_byte < static_cast<std::uint64_t>(m_memsize))
			{
				bool thebit = (m_mem[mem_byte] & (1 << (8-mem_bit-1))) != 0;

				// memory bit set?
				if(thebit)
					pixel[0] = 0xff;

				// mark instruction pointer position
				if(mem_byte == static_cast<std::uint64_t>(m_ip))
					pixel[1] |= 0xff;

				// mark current stack frame
				if(mem_byte >= static_cast<std::uint64_t>(m_sp) &&
					mem_byte <= static_cast<std::uint64_t>(m_bp))
					pixel[2] |= 0xff;
			}

			(*iter)[0] = pixel[0];
			(*iter)[1] = pixel[1];
			(*iter)[2] = pixel[2];
			++x;
		}
	}

	static std::size_t ctr = 0;
	std::string file = "mem_" + std::to_string(ctr++) + ".png";
	boost::gil::write_view(file, view, t_format{});
}


#else


/**
 * visualises vm memory utilisation (dummy function)
 */
void VM::DrawMemoryImage()
{
}

#endif
