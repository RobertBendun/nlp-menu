#include <string>
#include <string_view>

namespace utf8
{
	// TODO Only supports ASCII & Polish letters
	std::string to_lower(std::string_view sv)
	{
		static constexpr std::string_view multibyte_lower[] = { "ą", "ę", "ó", "ń", "ł", "ó", "ź", "ż", "ć", "ś" };
		static constexpr std::string_view multibyte_upper[] = { "Ą", "Ę", "Ó", "Ń", "Ł", "Ó", "Ź", "Ż", "Ć", "Ś" };

		std::string result;
next:
		while (!sv.empty()) {
			for (auto i = 0u; i < std::size(multibyte_lower); ++i) {
				if (sv.starts_with(multibyte_upper[i])) {
					sv.remove_prefix(multibyte_upper[i].size());
					result += multibyte_lower[i];
					goto next;
				}
			}
			result += std::tolower(sv.front());
			sv.remove_prefix(1);
		}
		return result;
	}

	std::pair<std::string_view, std::string_view> split_at_ws(std::string_view sv)
	{
		sv = trim(sv);
		auto ws = sv.find_first_of(" \t\n\r");
		if (ws == std::string_view::npos)
			return { sv, {} };
		return { trim(sv.substr(0, ws+1)), trim(sv.substr(ws+1)) };
	}
}

#if 0
#include <iostream>

int main()
{
	for (std::string line; (std::cout << "To lower: "), std::getline(std::cin, line); ) {
		std::cout << utf8::to_lower(line) << std::endl;
	}
}
#endif
