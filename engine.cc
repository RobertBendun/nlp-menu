#include <iostream>
#include <mutex>
#include <string_view>

#include <pybind11/embed.h>

namespace py = pybind11;

py::scoped_interpreter python;
py::module_ nlp;

std::once_flag python_initialized;

void on_input(std::string_view sv)
{
	std::call_once(python_initialized, [] {
		nlp = py::module_::import("nlp");
	});

	nlp.attr("echo")(sv);
}

extern "C"
{
	void on_input_callback(char const* s)
	{
		on_input(s);
	}
}
