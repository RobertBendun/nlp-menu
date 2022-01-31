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
#include <chrono>

#include "lisp.cc"
#include "unicode.cc"

namespace chrono = std::chrono;

void fill_items(std::vector<std::pair<std::string, Match const*>>& sugg)
{
	items = (struct item *)realloc(items, sizeof(struct item) * sugg.size());

	for (auto i = 0u; i < sugg.size(); ++i) {
		items[i].text = sugg[i].first.data();
		items[i].left = i == 0 ? nullptr : &items[i-1];
		items[i].right = i == sugg.size()-1 ? nullptr : &items[i+1];
		items[i].out = 0;
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

lisp::Value rules;
Suggestion_Tree tree;
std::once_flag tree_initialized;
std::vector<std::pair<std::string, Match const*>> suggestions;

Match *root = nullptr;

void on_input(std::string_view sv)
{
	std::string lowercase = utf8::to_lower(trim(sv));
	sv = lowercase;

	std::call_once(tree_initialized, [] {
		auto start = chrono::system_clock::now();
		std::ifstream stream("./wip.lisp");
		std::string file{std::istreambuf_iterator<char>(stream), {}};
		std::string_view code{file};

		rules = lisp::Value::list();
		rules.push_front(lisp::Value::symbol("do"));
		for (;;) {
			auto rule = lisp::read(code);
			if (rule.kind == lisp::Value::Kind::Nil)
				break;
			rules.push_back(std::move(rule));
		}

		tree.eval(rules);
		tree.optimize();
		auto end = chrono::system_clock::now();

		std::cout << "LISP initialization took " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms" << std::endl;
	});

	suggestions.clear();
	root = &tree;

	std::vector<std::string_view> substrings_to_match;

	std::string_view next = sv;
outer:
	for (;;) {
		// Skip empty nodes
		while (std::get_if<std::monostate>(root) && root->next.size() == 1) root = root->next.front().get();

		std::tie(sv, next) = utf8::split_at_ws(next);
		if (sv.empty()) {
			// TODO Walk tree to get good subset of suggestions
			for (auto const& c : root->next) {
				auto var = c.get();

				if (auto x = std::get_if<std::string>(var)) {
					suggestions.push_back({ *x, var });
					continue;
				}

				if (auto x = std::get_if<fs::path>(var)) {
					suggestions.push_back({ x->filename().string(), var });
					continue;
				}
			}
			return fill_items(suggestions);
		}

		for (auto const& c : root->next) {
			auto var = c.get();

			if (auto x = std::get_if<fs::path>(var)) {
				auto fname = utf8::to_lower(x->filename().string());

				for (auto substr : substrings_to_match)
					if (fname.find(substr) == std::string::npos)
						goto skip_path;

				if (fname.find(sv) != std::string::npos) {
					suggestions.push_back({ fname, var });
					continue;
				}
			}
skip_path:

			if (auto x = std::get_if<std::string>(var)) {
				auto [g, e] = std::mismatch(x->begin(), x->end(), sv.begin(), sv.end());

				if (g == x->begin() && e == sv.begin())
					continue;

				if (g != x->cend() && e == sv.end()) {
					suggestions.push_back({ *x, var });
					continue;
				}

				if (g == x->cend() && e == sv.end()) {
					root = c.get();
					goto outer;
				}

				if (g == x->cend() && e != sv.end()) {
					error("0 1 not implemented yet");
				}
			}
		}

		if (!next.empty()) {
			suggestions.clear();
			substrings_to_match.push_back(sv);
			goto outer;
		}

		return fill_items(suggestions);
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

		auto found = std::find_if(suggestions.begin(), suggestions.end(), [](auto const& el) {
			return el.first.data() == sel->text; });

		assert(found != suggestions.end());
		assert(found->second->command);
		auto command = found->second->eval();

		int pipe[2];
		::pipe(pipe);

		close(STDIN_FILENO);
		dup2(pipe[0], STDIN_FILENO);
		close(pipe[0]);

		close(STDOUT_FILENO);
		dup2(pipe[1], STDOUT_FILENO);
		std::cout << command << std::endl;
		close(pipe[1]);
		close(STDOUT_FILENO);

		cleanup();
		execl("/bin/sh", "/bin/sh", (char*)nullptr);
		exit(1);
	}
}
