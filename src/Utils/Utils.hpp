#ifndef UTILS_HPP
# define UTILS_HPP

#include "structs.hpp"

class Utils {
	private:
		Utils();
		Utils(const Utils& other);
		Utils& operator=(const Utils& other);
		~Utils();
	
	public:
		static std::string	trim(const std::string& str);
		static void			ref_trim(std::string& str);
};

#endif