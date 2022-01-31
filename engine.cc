#include "engine.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string_view>
#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <atomic>

#include <sys/wait.h>
#include <unistd.h>
#include <regex>

#include "lisp.cc"

void fill_items(std::vector<std::string>& sugg)
{
	items = (struct item *)realloc(items, sizeof(struct item) * sugg.size());

	for (auto i = 0u; i < sugg.size(); ++i) {
		items[i].text = sugg[i].data();
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

Suggestion_Tree tree;
std::once_flag tree_initialized;
std::vector<std::string> suggestions;

Match *root = nullptr;

void on_input(std::string_view sv)
{
	std::call_once(tree_initialized, [] {
		std::ifstream stream("./wip.lisp");
		std::string file{std::istreambuf_iterator<char>(stream), {}};
		std::string_view code{file};


		auto rules = lisp::Value::list();
		rules.push_front(lisp::Value::symbol("do"));
		for (;;) {
			auto rule = lisp::read(code);
			if (rule.kind == lisp::Value::Kind::Nil)
				break;
			rules.push_back(std::move(rule));
		}

		tree.eval(rules);
		tree.optimize();
	});

	suggestions.clear();

	if (root == nullptr) { root = &tree; }
	// Skip empty nodes
	while (std::get_if<std::monostate>(root) && root->next.size() == 1) root = root->next.front().get();

	// TODO Walk tree to get good subset of suggestions
	for (auto const& c : root->next) {
		auto var = c.get();
		if (auto x = std::get_if<std::string>(var)) { suggestions.push_back(*x); continue; }
		if (auto x = std::get_if<fs::path>(var)) { suggestions.push_back(x->filename().string()); continue; }
	}

	fill_items(suggestions);
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
		// place suggestion
		close(STDOUT_FILENO);

		cleanup();
		execl("/bin/sh", "/bin/sh", (char*)nullptr);
		exit(1);
	}
}
