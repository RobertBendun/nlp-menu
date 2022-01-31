#include <cassert>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <variant>
#include <vector>

#include "os-exec/os-exec.hh"

namespace fs = std::filesystem;

using namespace std::string_literals;
using namespace std::string_view_literals;

[[noreturn]]
void error(std::string_view message)
{
	std::cerr << "ERROR: " << message << std::endl;
	std::exit(1);
}

void ensure(bool cond, std::string_view message)
{
	if (cond) return;
	error(message);
}

[[nodiscard]]
std::string_view trim(std::string_view sv)
{
	while (!sv.empty() && std::isspace(sv.front())) sv.remove_prefix(1);
	while (!sv.empty() && std::isspace(sv.back())) sv.remove_suffix(1);
	return sv;
}

namespace lisp
{
	using uint = unsigned long long;

	struct Value : std::list<Value>
	{
		enum class Kind
		{
			Nil,
			String,
			Number,
			Symbol,
			List
		} kind = Kind::Nil;
		std::string str;
		uint num;

		static Value list() { Value v; v.kind = Kind::List; return v; }
		static Value number(uint n = 0) { Value v; v.kind = Kind::Number; v.num = n; return v; }
		static Value string(std::string s = {}) { Value v; v.kind = Kind::String; v.str = std::move(s); return v; }
		static Value symbol(std::string s = {}) { Value v; v.kind = Kind::Symbol; v.str = std::move(s); return v; }

		bool is_call_to(std::string_view sv) const {
			return kind == Kind::List && !empty() &&
				front().kind == Kind::Symbol && front().str == sv; }
	};

	Value read(std::string_view &s)
	{
		while (!s.empty()) {
			for (; !s.empty() && std::isspace(s.front()); s.remove_prefix(1)) {}
			if (s.starts_with(';')) { s.remove_prefix(s.find('\n')); } else { break; }
		}

		if (s.empty()) return {};

		if (s.starts_with('"')) {
			auto end = std::adjacent_find(s.cbegin()+1, s.cend(), [](char p, char c) { return p != '\\' && c == '"'; });
			auto str = Value::string({ s.cbegin()+1, end+1 });
			s.remove_prefix(std::distance(s.cbegin(), end+2));
			return str;
		}

		if (std::isdigit(s.front())) {
			auto v = Value::number();
			auto [p, _] = std::from_chars(&s.front(), &s.back()+1, v.num);
			s.remove_prefix(p - &s.front());
			return v;
		}


		if (s.front() == '(') {
			auto list = Value::list();
			s.remove_prefix(1);
			for (Value elem; (elem = read(s)).kind != Value::Kind::Nil; list.push_back(std::move(elem))) {}
			return list;
		}

		if (s.starts_with(')')) {
			s.remove_prefix(1);
			return {};
		}

		auto constexpr issymbol = [](char c) { return !std::isspace(c) && std::string_view("()\"").find(c) == std::string_view::npos; };

		if (issymbol(s.front())) {
			auto end = std::find_if_not(s.cbegin()+1, s.cend(), issymbol);
			auto symbol = Value::symbol({ s.cbegin(), end });
			s.remove_prefix(end - s.cbegin());
			return symbol;
		}

		return {};
	}

	Value dump(Value const& v, uint indent = 0)
	{
		std::cout << std::string(indent, ' ');
		switch (v.kind) {
		case Value::Kind::Nil: std::cout << "NIL\n"; break;
		case Value::Kind::Number: std::cout << "NUM " << v.num << "\n"; break;
		case Value::Kind::String: std::cout << "STR " << std::quoted(v.str) << "\n"; break;
		case Value::Kind::Symbol: std::cout << "SYM " << std::quoted(v.str) << "\n"; break;
		case Value::Kind::List: std::cout << "LST " << v.size() << "\n";
			for (auto const& el : v)
				dump(el, indent+2);
			break;
		}

		return {};
	}

}

fs::path resolve_home(fs::path path)
{
	auto it = path.begin();

	if (*it != "~") return path;

	auto home = getenv("HOME");
	assert(home);
	fs::path resolved = fs::path(home);
	while (++it != path.end()) { resolved /= *it; }
	return resolved;
}

std::vector<fs::path> find_dirs(fs::path root)
{
	std::map<fs::path, std::vector<fs::path>> cache;
	if (auto m = cache.find(root); m != cache.end())
		return m->second;

	std::vector<fs::path> paths;

	for (auto entry : fs::directory_iterator(resolve_home(root)))
		if (entry.is_directory())
			paths.push_back(fs::absolute(entry.path()));

	cache.insert({ root, paths });
	return paths;
}

std::vector<fs::path> find_all_executable(fs::path root)
{
	std::map<fs::path, std::vector<fs::path>> cache;
	if (auto m = cache.find(root); m != cache.end())
		return m->second;

	std::vector<fs::path> paths;

	for (auto entry : fs::recursive_directory_iterator(resolve_home(root)))
		if (entry.is_regular_file() && (entry.status().permissions() & fs::perms::owner_exec) != fs::perms::none)
			paths.push_back(fs::absolute(entry.path()));

	cache.insert({ root, paths });
	return paths;
}

std::vector<fs::path> find_with_extension(fs::path root, std::vector<std::string_view> extensions)
{
	std::vector<fs::path> paths;

	for (auto entry : fs::recursive_directory_iterator(resolve_home(root))) {
		if (!entry.is_regular_file()) continue;
		std::string_view ext = entry.path().extension().c_str();
		if (ext.starts_with('.') && std::find(extensions.begin(), extensions.end(), ext.substr(1)) != extensions.cend())
			paths.push_back(fs::absolute(entry.path()));
	}

	return paths;
}

std::vector<fs::path> find_all_processes()
{
	std::set<fs::path> paths;

	for (auto entry : fs::recursive_directory_iterator("/proc/", fs::directory_options::skip_permission_denied)) {
		auto p = entry.path();
		if (p.filename() == "exe") {
			try {
				paths.insert(fs::canonical(p));
			} catch (std::exception const&) {}
		}
	}

	return { paths.begin(), paths.end() };
}

using Match_Variant = std::variant<std::monostate, std::string, std::regex, fs::path>;

struct Match : Match_Variant
{
	std::vector<std::unique_ptr<Match>> next{};
	lisp::Value const* command = nullptr;

	auto const& as_variant() const { return *static_cast<Match_Variant const*>(this); }

	Match* put() { return next.emplace_back(std::make_unique<Match>()).get(); }

	auto operator==(Match const& other) const
	{
		if (auto p = std::get_if<std::monostate>(this), q = std::get_if<std::monostate>(&other); p && q) return true;
		if (auto p = std::get_if<std::string>(this), q = std::get_if<std::string>(&other); p && q) return *p == *q;
		if (auto p = std::get_if<fs::path>(this), q = std::get_if<fs::path>(&other); p && q) return *p == *q;
		return false;
	}

	std::string eval() const
	{
		assert(command);

		auto result = std::string{};

		for (auto const& v : *command) {
			switch (v.kind) {
			case lisp::Value::Kind::Nil: continue;
			case lisp::Value::Kind::Number: continue;
			case lisp::Value::Kind::String: result += " " + os_exec::shell_quote(v.str); break;
			case lisp::Value::Kind::Symbol:
				if (v.str == "last") {
					if (auto x = std::get_if<std::string>(this)) { result += " " + os_exec::shell_quote(*x); continue; }
					if (auto x = std::get_if<fs::path>(this)) { result += " " + os_exec::shell_quote(x->string()); continue; }
					error("this type is not supported yet");
				}
				error("unsupported symbol name");
			case lisp::Value::Kind::List:
				error("Execution of list is not supported yet");
			}
		}

		return result;
	}

	// Evaluates rule, to this node and passes unevaluated to children
	Match* eval(lisp::Value::const_iterator rule, lisp::Value::const_iterator rule_end, lisp::Value const* command)
	{
		using namespace lisp;
		switch (rule->kind) {
		case Value::Kind::Nil:    error("Nil cannot be rule");
		case Value::Kind::Number: error("Number cannot be rule");
		case Value::Kind::Symbol:
			{
				if (rule->str == "match-email") error("Matching email is not implemented yet");
				if (rule->str == "match-url") error("matching url is not implemented yet");
				error("Uncrecognized symbol");
			}
			break;
		case Value::Kind::String:
			emplace<std::string>(rule->str);
			break;
		case Value::Kind::List:
			if (rule->is_call_to("one-of")) {
				auto cmd = std::next(rule) == rule_end ? command : nullptr;
				for (auto posibility = std::next(rule->cbegin()); posibility != rule->cend(); ++posibility) {
					auto next = put()->eval(posibility, std::next(posibility), cmd);
					if (std::next(rule) != rule_end) next->put()->eval(std::next(rule), rule_end, command);
				}
				return this;
			}

			if (rule->is_call_to("find-dirs")) {
				auto arg = rule->cbegin();
				auto const& root = (++arg)->str;

				auto paths = find_dirs(root);

				for (auto path : paths) {
					auto next = put();
					next->emplace<fs::path>(std::move(path));
					if (std::next(rule) != rule_end)
						next->put()->eval(std::next(rule), rule_end, command);

					if (std::next(rule) == rule_end)
						next->command = command;
				}

				return this;
			}

			if (rule->is_call_to("processes")) {
				auto paths = find_all_processes();

				for (auto path : paths) {
					auto next = put();
					next->emplace<fs::path>(std::move(path));
					if (std::next(rule) != rule_end)
						next->put()->eval(std::next(rule), rule_end, command);

					if (std::next(rule) == rule_end)
						next->command = command;
				}

				return this;
			}

			if (rule->is_call_to("find-all-executable")) {
				auto arg = rule->cbegin();
				auto const& root = (++arg)->str;

				auto paths = find_all_executable(root);

				for (auto path : paths) {
					auto next = put();
					next->emplace<fs::path>(std::move(path));
					if (std::next(rule) != rule_end)
						next->put()->eval(std::next(rule), rule_end, command);

					if (std::next(rule) == rule_end)
						next->command = command;
				}

				return this;
			}

			if (rule->is_call_to("find-all-with-extension")) {
				auto arg = rule->cbegin();
				auto const& extensions_declaration = *++arg;
				auto const& root = (++arg)->str;

				std::vector<std::string_view> extensions;
				for (auto const& ext : extensions_declaration) {
					ensure(ext.kind == lisp::Value::Kind::String, "Extensions group must be all strings");
					extensions.push_back(ext.str);
				}
				auto paths = find_with_extension(root, std::move(extensions));

				for (auto path : paths) {
					auto next = put();
					next->emplace<fs::path>(std::move(path));
					if (std::next(rule) != rule_end)
						next->put()->eval(std::next(rule), rule_end, command);

					if (std::next(rule) == rule_end)
						next->command = command;
				}

				return this;
			}

			error("Unrecognized function call");
			break;
		}

		if (++rule != rule_end) return put()->eval(rule, rule_end, command);
		this->command = command;
		return this;
	}

	bool optimize()
	{
		bool try_again = true;
		bool done_something = false;

		while (try_again) {
			try_again = false;

			for (auto &child : next)
				try_again |= child->optimize();

			// For all possible unordered pairs try unification
			for (unsigned i = 0; i < next.size(); ++i) {
				for (unsigned j = i+1; j < next.size(); ++j) {
					if (*next[i] == *next[j]) {
						std::move(next[j]->next.begin(), next[j]->next.end(), std::back_inserter(next[i]->next));
						next.erase(next.begin() + j);
						done_something = try_again = true;
					}
				}
				try_again |= next[i]->optimize();
			}

			if (next.size() == 1 && std::get_if<std::monostate>(next.front().get())) {
				auto succ = std::move(next.front());
				next.clear();
				next.reserve(succ->next.size());
				std::move(succ->next.begin(), succ->next.end(), std::back_inserter(next));
				done_something = try_again = true;
			}

			if (std::get_if<std::monostate>(this)) {
				for (bool revisit = true; revisit;) {
					revisit = false;
					for (auto i = 0u; i < next.size(); ++i) {
						if (std::get_if<std::monostate>(next[i].get())) {
							std::move(next[i]->next.begin(), next[i]->next.end(), std::back_inserter(next));
							next.erase(next.begin() + i);
							try_again = true;
							revisit = true;
							break;
						}
					}
				}
			}
		}

		return done_something;
	}
};

template<class...Fs> struct overload : Fs... { using Fs::operator()...; };
template<class...Fs> overload(Fs...) -> overload<Fs...>;

void dump(Match const& match)
{
	std::cout << "digraph Suggestion_Tree {\n";

	std::stack<Match const*> stack;
	stack.push(&match);

	while (!stack.empty()) {
		auto top = stack.top();
		stack.pop();

		std::cout << "Node_" << std::hex << top;
		std::visit(overload{
			[](std::monostate)       { std::cout << " [label=\"<>\"];\n"; },
			[](std::string const& s) { std::cout << " [label=" << std::quoted(s) << "];\n"; },
			[](fs::path const& f)    { std::cout << " [label=" << f.filename() << "];\n"; },
			[](std::regex const&)    { std::cout << " [label=regex];\n"; },
		}, top->as_variant());

		if (top->command) {
			std::cout << "Cmd_" << std::hex << top->command << " [shape=box,label=" << std::quoted(top->command->front().str) << "];\n";
			std::cout << "Node_" << std::hex << top << " -> Cmd_" << std::hex << top->command << ";\n";
		}

		for (auto const& ch : top->next) {
			auto child = ch.get();
			std::cout << "Node_" << std::hex << top << " -> Node_" << std::hex << child << ";\n";
			stack.push(child);
		}
	}
	std::cout << "}" << std::endl;
}

struct Suggestion_Tree : Match
{
	void eval(lisp::Value const&);
};

void Suggestion_Tree::eval(lisp::Value const& v)
{
	Suggestion_Tree tree;

	ensure(v.is_call_to("do"), "Expected do call");

	for (auto action = std::next(v.cbegin()); action != v.cend(); ++action) {
		ensure(action->is_call_to("action"), "Expected action call");
		ensure(action->size() == 3, "Action requires rule definition and command declaration");

		auto args = action->cbegin();
		auto const &rule = *++args;
		auto const command = &*++args;
		put()->eval(rule.cbegin(), rule.cend(), command);
	}
}

#ifdef Main
int main(int, char **argv)
{
	assert(*++argv);

	std::ifstream f(*argv);
	std::string file{std::istreambuf_iterator<char>(f), {}};

	std::string_view code{file};

	auto rules = lisp::Value::list();
	rules.push_front(lisp::Value::symbol("do"));
	for (;;) {
		auto rule = lisp::read(code);
		if (rule.kind == lisp::Value::Kind::Nil)
			break;
		rules.push_back(std::move(rule));
	}
	//dump(rules);

	Suggestion_Tree tree;
	tree.eval(rules);
	tree.optimize();
	dump(tree);
}
#endif
