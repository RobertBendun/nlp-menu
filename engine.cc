#include "engine.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string_view>
#include <vector>
#include <utility>

#include <pybind11/embed.h>
#include <signal.h>
#include <sys/wait.h>

namespace py = pybind11;

py::scoped_interpreter *python = nullptr;

py::module_ nlp;

std::once_flag python_initialized;

std::vector<std::pair<std::string, py::object>> sugg;

void on_input(std::string_view sv)
{
	std::call_once(python_initialized, [] {
		python = new py::scoped_interpreter;
		nlp = py::module_::import("client");
	});

	auto suggestions = nlp.attr("get_suggestions")(sv).cast<py::list>();

	sugg.clear();

	for (auto s : suggestions) {
		auto tup = s.cast<py::tuple>();
		assert(tup.size() == 2);
		sugg.push_back({ tup[0].cast<std::string>(), tup[1] });
	}

	items = (struct item *)realloc(items, sizeof(struct item) * sugg.size());

	for (auto i = 0u; i < sugg.size(); ++i) {
		items[i].text = sugg[i].first.data();
		items[i].left = i == 0 ? nullptr : &items[i-1];
		items[i].right = i == sugg.size()-1 ? nullptr : &items[i+1];
	}

	if (sugg.empty()) {
		sel = curr = matches = items = matchend = nullptr;
	} else {
		sel = curr = matches = items;
		matchend = items + sugg.size();
		prev = nullptr;
		next = items->right;
	}
}

extern "C"
{
	void on_input_callback(char const* s)
	{
		on_input(s);
	}

	void choose()
	{
		if (!sel) {
			cleanup();
			exit(1);
		}

		int pipe[2];
		::pipe(pipe);

		close(STDIN_FILENO);
		dup2(pipe[0], STDIN_FILENO);
		close(pipe[0]);

		close(STDOUT_FILENO);
		dup2(pipe[1], STDOUT_FILENO);
		close(pipe[1]);
		sugg[sel - items].second();
		close(STDOUT_FILENO);

		cleanup();
		execl("/bin/sh", "/bin/sh", (char*)nullptr);
		exit(1);
	}
}
