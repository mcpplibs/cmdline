module;

export module mcpplibs.cmdline:completions.fish;

import std;
import :completions;

namespace {

/// Replace '-' with '_' in fish function names.
[[nodiscard]] std::string escape_name(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) result += (c == '-') ? '_' : c;
    return result;
}

/// Escape string inside single quotes.
[[nodiscard]] std::string escape_string(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if (c == '\\')
            result += "\\\\";
        else if (c == '\'')
            result += "\\'";
        else if (c == '\n')
            result += ' ';
        else
            result += c;
    }
    return result;
}

/// Generate an argparse optspec string for one option.
[[nodiscard]] std::string optspec_for_option(const mcpplibs::cmdline::detail::Option& opt) {
    std::string spec;
    if (opt.short_) {
        spec += opt.short_;
        if (!opt.long_name.empty()) {
            spec += '/';
            spec += opt.long_name;
        }
    } else {
        spec += opt.long_name;
    }
    if (opt.takes_value_) spec += '=';
    return spec;
}

/// Build the condition string for `-n`.
[[nodiscard]] std::string build_condition(
    std::string_view escaped,
    const std::vector<std::string>& parent_path,
    std::span<const std::string> child_names)
{
    if (parent_path.empty()) return std::format("__fish_{}_needs_command", escaped);

    if (parent_path.size() == 1) {
        auto condition = std::format("__fish_{}_using_subcommand {}", escaped, parent_path[0]);
        if (!child_names.empty()) {
            condition += "; and not __fish_seen_subcommand_from";
            for (const auto& child_name : child_names) {
                condition += ' ';
                condition += child_name;
            }
        }
        return condition;
    }

    if (parent_path.size() == 2) {
        return std::format(
            "__fish_{}_using_subcommand {}; and __fish_seen_subcommand_from {}",
            escaped, parent_path[0], parent_path[1]
        );
    }

    // HACK: Cases with depth >= 3 take much effort to support. Emit nothing.
    return {};
}

void gen_subcommand_helpers(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view escaped,
    std::ostream& out)
{
    std::vector<std::string> optspecs;
    optspecs.reserve(cmd.options.size());
    for (const auto& opt : cmd.options) optspecs.emplace_back(optspec_for_option(opt));

    out << "function __fish_" << escaped << "_global_optspecs\n";
    out << "    string join \\n";
    for (const auto& optspec : optspecs) out << ' ' << optspec;
    out << '\n';
    out << "end\n\n";

    out << "function __fish_" << escaped << "_needs_command\n";
    out << "    set -l cmd (commandline -opc)\n";
    out << "    set -e cmd[1]\n";
    out << "    argparse -s (__fish_" << escaped << "_global_optspecs) -- $cmd 2>/dev/null\n";
    out << "    or return\n";
    out << "    if set -q argv[1]\n";
    out << "        echo $argv[1]\n";
    out << "        return 1\n";
    out << "    end\n";
    out << "    return 0\n";
    out << "end\n\n";

    out << "function __fish_" << escaped << "_using_subcommand\n";
    out << "    set -l cmd (__fish_" << escaped << "_needs_command)\n";
    out << "    test -z \"$cmd\"\n";
    out << "    and return 1\n";
    out << "    contains -- $cmd[1] $argv\n";
    out << "end\n\n";
}

}; // unnamed namespace

namespace mcpplibs::cmdline::completions::fish {

void emit_option_line(
    std::string_view root_name,
    std::string_view condition,
    const detail::Option& opt,
    std::ostream& out)
{
    out << "complete -c " << root_name;
    if (!condition.empty()) out << " -n '" << condition << '\'';
    if (opt.short_) out << " -s " << opt.short_;
    if (!opt.long_name.empty()) out << " -l " << opt.long_name;
    if (opt.takes_value_) out << " -r";
    if (!opt.help_.empty()) out << " -d '" << escape_string(opt.help_) << '\'';
    out << '\n';
}

void emit_subcommand_line(
    std::string_view root_name,
    std::string_view condition,
    const Command& sub,
    bool has_positionals,
    std::ostream& out)
{
    out << "complete -c " << root_name;
    if (!condition.empty()) out << " -n '" << condition << '\'';
    if (!has_positionals) out << " -f";
    out << " -a \"" << sub.name << '"';
    if (!sub.description.empty()) out << " -d '" << escape_string(sub.description) << '\'';
    out << '\n';
}

void gen_inner(
    std::string_view root_name,
    std::string_view escaped,
    const std::vector<std::string>& parent_path,
    const Command& node,
    std::span<const detail::Option> root_globals,
    std::ostream& out)
{
    std::vector<std::string> child_names;
    child_names.reserve(node.subcommands.size());
    for (const auto& sub : node.subcommands) child_names.emplace_back(sub.name);

    auto condition = build_condition(escaped, parent_path, child_names);
    if (condition.empty()) {
        // Depth >= 3: it takes much effort to support. Emit nothing.
        return;
    }

    // Options for this node
    for (const auto& opt : node.options) emit_option_line(root_name, condition, opt, out);

    // Global re-offer (non-root)
    if (!parent_path.empty()) {
        for (const auto& rg : root_globals) {
            // Skip if this node already has an option with the same long or short name.
            bool already_present = false;
            const auto& rg_long = rg.long_name;
            char rg_short = rg.short_;
            for (const auto& existing : node.options) {
                if ((!rg_long.empty() && existing.long_name == rg_long) ||
                    (!rg_short        && existing.short_    == rg_short))
                {
                    already_present = true;
                    break;
                }
            }
            if (!already_present) emit_option_line(root_name, condition, rg, out);
        }
    }

    // Subcommands for this node
    for (const auto& sub : node.subcommands) emit_subcommand_line(root_name, condition, sub, node.has_positionals(), out);

    // Recurse into children
    for (const auto& sub : node.subcommands) {
        auto child_path = parent_path;
        child_path.emplace_back(sub.name);
        gen_inner(root_name, escaped, child_path, sub, root_globals, out);
    }
}


/// Emit one `complete` line per option.  No `-n` condition. No helpers.
void generate_simple(const completions::Command& cmd, std::ostream& out) {
    for (const auto& opt : cmd.options) emit_option_line(cmd.name, {}, opt, out);
}


export void generate(const Command& cmd, std::ostream& out) {
    if (!cmd.has_subcommands()) {
        generate_simple(cmd, out);
        return;
    }

    // Collect root globals for re-offer under subcommands.
    std::vector<detail::Option> root_globals;
    for (const auto& opt : cmd.options) {
        if (opt.global_) root_globals.emplace_back(opt);
    }

    auto escaped = escape_name(cmd.name);
    gen_subcommand_helpers(cmd, escaped, out);
    gen_inner(cmd.name, escaped, {}, cmd, root_globals, out);
}

}; // namespace mcpplibs::cmdline::completions::fish
