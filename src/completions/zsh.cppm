module;

export module mcpplibs.cmdline:completions.zsh;

import std;
import :completions;

namespace mcpplibs::cmdline::completions::zsh::detail {

/// An entry in the flattened subcommand tree: the function-name path of the
/// parent, the subcommand's display name, and the subcommand's own
/// function-name path (using escaped component names for valid zsh identifiers).
struct SubCmdEntry {
    std::string parent_fn;
    std::string name;
    std::string fn;
};

} // namespace mcpplibs::cmdline::completions::zsh::detail

namespace {

using mcpplibs::cmdline::completions::zsh::detail::SubCmdEntry;

/// Separator used to join subcommand names in the internal path representation
/// (e.g. `"myapp__subcmd__install__subcmd__config"`).
constexpr std::string_view path_sep = "__subcmd__";

/// Replace '-' with '_' so the result is a valid zsh function name.
[[nodiscard]] std::string escape_name(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) result += (c == '-') ? '_' : c;
    return result;
}

/// Escape help string for use inside `'spec[help]'` in `_arguments`.
/// Escapes `\`, `'`, `[`, `]`, `:`, `$`, `` ` ``, and replaces `\n` with space.
[[nodiscard]] std::string escape_help(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '\'': result += "'\\''"; break;
            case '[':  result += "\\[";  break;
            case ']':  result += "\\]";  break;
            case ':':  result += "\\:";  break;
            case '$':  result += "\\$";  break;
            case '`':  result += "\\`";  break;
            case '\n': result += ' ';    break;
            default:   result += c;      break;
        }
    }
    return result;
}

/// Escape value string for use inside `'spec:value:action'` in `_arguments`.
/// Same as `escape_help` plus escape `(`, `)`, and space.
[[nodiscard]] std::string escape_value(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '\'': result += "'\\''"; break;
            case '[':  result += "\\[";  break;
            case ']':  result += "\\]";  break;
            case ':':  result += "\\:";  break;
            case '$':  result += "\\$";  break;
            case '`':  result += "\\`";  break;
            case '(':  result += "\\(";  break;
            case ')':  result += "\\)";  break;
            case ' ':  result += "\\ ";  break;
            case '\n': result += ' ';    break;
            default:   result += c;      break;
        }
    }
    return result;
}

/// Navigate the Command tree by `path_sep`-separated path components.
/// Returns a pointer to the target subcommand, or the root command when path
/// is empty.  Returns `nullptr` when a component is not found (should not
/// happen with consistent input from `flatten_subcommands`).
[[nodiscard]] const mcpplibs::cmdline::completions::Command* walk_command(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view path)
{
    if (path.empty()) return &cmd;

    const auto* node = &cmd;
    std::size_t start = 0;
    while (start < path.size()) {
        auto end = path.find(path_sep, start);
        std::string_view component;
        if (end == std::string_view::npos) {
            component = path.substr(start);
            start = path.size();
        } else {
            component = path.substr(start, end - start);
            start = end + path_sep.size();
        }
        if (component.empty()) continue;

        bool found = false;
        for (const auto& sub : node->subcommands) {
            if (sub.name == component) {
                node = &sub;
                found = true;
                break;
            }
        }
        if (!found) return nullptr;
    }
    return node;
}

void flatten_subcommands_impl(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view parent_fn,
    std::vector<SubCmdEntry>& out)
{
    for (const auto& sub : cmd.subcommands) {
        SubCmdEntry entry;
        entry.parent_fn = std::string(parent_fn);
        entry.name = sub.name;
        entry.fn = std::string(parent_fn) + std::string(path_sep) + escape_name(sub.name);
        out.push_back(entry);
        flatten_subcommands_impl(sub, entry.fn, out);
    }
}

/// Flatten the subcommand tree into a list of `SubCmdEntry` tuples, one per
/// subcommand at every level.
[[nodiscard]] std::vector<SubCmdEntry> flatten_subcommands(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view parent_fn)
{
    std::vector<SubCmdEntry> out;
    flatten_subcommands_impl(cmd, parent_fn, out);
    return out;
}

/// Check whether a subcommand's own options already include an option with
/// the given short or long name, for global re-offer dedup.
[[nodiscard]] bool has_option(
    const mcpplibs::cmdline::completions::Command& node,
    char short_,
    std::string_view long_name)
{
    for (const auto& opt : node.options) {
        if (!long_name.empty() && opt.long_name == long_name) return true;
        if (short_ && opt.short_ == short_) return true;
    }
    return false;
}

/// Build `_arguments` spec lines for all options of the given node.
/// Root globals are re-offered when `is_root` is false (i.e., for subcommand
/// nodes), deduped against the node's own options.
[[nodiscard]] std::string write_options(
    const mcpplibs::cmdline::completions::Command& cmd,
    bool is_root,
    std::span<const mcpplibs::cmdline::detail::Option> root_globals)
{
    std::vector<const mcpplibs::cmdline::detail::Option*> opts;
    for (const auto& opt : cmd.options) opts.emplace_back(&opt);

    if (!is_root) {
        for (const auto& rg : root_globals) {
            if (has_option(cmd, rg.short_, rg.long_name)) continue;
            opts.emplace_back(&rg);
        }
    }

    std::string out;
    for (const auto* opt : opts) {
        auto help = opt->help_.empty() ? "" : escape_help(opt->help_);
        auto value_name = opt->value_name_.empty() ? " " : opt->value_name_;

        if (opt->short_) {
            out += "        '";
            if (opt->takes_value_) {
                out += '-';
                out += opt->short_;
                out += '+';
                if (!help.empty()) out += '[' + help + ']';
                out += ':';
                out += value_name;
                out += ":_default";
            } else {
                out += '-';
                out += opt->short_;
                if (!help.empty()) out += '[' + help + ']';
            }
            out += "' \\\n";
        }

        if (!opt->long_name.empty()) {
            out += "        '";
            if (opt->takes_value_) {
                out += "--";
                out += opt->long_name;
                out += '=';
                if (!help.empty()) out += '[' + help + ']';
                out += ':';
                out += value_name;
                out += ":_default";
            } else {
                out += "--";
                out += opt->long_name;
                if (!help.empty()) out += '[' + help + ']';
            }
            out += "' \\\n";
        }
    }
    return out;
}

/// Build `_arguments` spec lines for positionals.
[[nodiscard]] std::string write_positionals(const mcpplibs::cmdline::completions::Command& cmd) {
    std::string out;
    for (const auto& arg : cmd.args) {
        auto help = arg.help_.empty() ? "" : escape_help(" -- " + arg.help_);
        // Required → single colon prefix; optional → double colon prefix
        auto cardinality = arg.required_ ? "" : ":";
        out += "        '";
        out += cardinality;
        out += ':';
        out += arg.name;
        out += help;
        out += ":_default' \\\n";
    }
    return out;
}

/// List direct subcommands in zsh's `commands` array format:
/// `'name:description'`.
/// One line per subcommand (no aliases).
[[nodiscard]] std::string subcommands_of(const mcpplibs::cmdline::completions::Command& cmd) {
    if (cmd.subcommands.empty()) return {};

    std::string out;
    for (const auto& sub : cmd.subcommands) {
        auto desc = sub.description.empty() ? "" : escape_help(sub.description);
        out += "        '";
        out += sub.name;
        out += ':';
        out += desc;
        out += "' \\\n";
    }
    return out;
}

/// Build the `_arguments ... && ret=0` block for a node.
/// `escaped_fn` is the escaped `path_sep`-joined name used for the
/// `_commands` helper-function reference (empty for root means no ref).
[[nodiscard]] std::string get_args_of(
    const mcpplibs::cmdline::completions::Command& cmd,
    bool is_root,
    std::span<const mcpplibs::cmdline::detail::Option> root_globals,
    std::string_view escaped_fn)
{
    auto opts = write_options(cmd, is_root, root_globals);
    auto positionals = write_positionals(cmd);

    std::string out;
    out += "    _arguments \"${_arguments_options[@]}\" : \\\n";
    out += opts;
    out += positionals;

    if (cmd.has_subcommands()) {
        out += "        \":: :_";
        out += escaped_fn;
        out += "_commands\" \\\n";
        out += "        \"*::: :->";
        out += cmd.name;
        out += "\" \\\n";
    }

    out += "    && ret=0";
    return out;
}

/// Build the `case $state in` dispatch block for subcommands of `cmd`.
/// `fn` is the RAW `path_sep`-joined path from root (used for tree
/// navigation via `walk_command`).
/// `escaped_fn` is the ESCAPED path (used for zsh function names).
/// Recursively generates nested state cases for deeper subcommand levels.
[[nodiscard]] std::string get_subcommands_of(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view fn,
    std::string_view escaped_fn,
    std::span<const mcpplibs::cmdline::detail::Option> root_globals)
{
    if (cmd.subcommands.empty()) return {};

    // Build hyphenated name for curcontext: replace path_sep with '-'
    std::string name_hyphen;
    if (fn.empty()) {
        name_hyphen = cmd.name;
    } else {
        name_hyphen = std::string(fn);
        std::size_t pos = 0;
        while ((pos = name_hyphen.find(path_sep, pos)) != std::string::npos) {
            name_hyphen.replace(pos, path_sep.size(), "-");
            pos += 1;
        }
        name_hyphen += '-';
        name_hyphen += cmd.name;
    }

    std::string all_subcommands;
    for (const auto& sub : cmd.subcommands) {
        // Raw path for tree navigation
        auto sub_fn = std::string(fn);
        if (!sub_fn.empty()) sub_fn += std::string(path_sep);
        sub_fn += sub.name;

        // Escaped path for zsh function names
        auto sub_escaped_fn = std::string(escaped_fn);
        if (!sub_escaped_fn.empty()) sub_escaped_fn += std::string(path_sep);
        sub_escaped_fn += escape_name(sub.name);

        all_subcommands += "            (";
        all_subcommands += sub.name;
        all_subcommands += ")\n";

        auto args = get_args_of(sub, false, root_globals, sub_escaped_fn);
        if (!args.empty()) {
            all_subcommands += args;
            all_subcommands += "\n";
        }

        auto children = get_subcommands_of(sub, sub_fn, sub_escaped_fn, root_globals);
        if (!children.empty()) {
            all_subcommands += children;
        }

        all_subcommands += "            ;;\n";
    }

    auto pos = cmd.args.size() + 1;

    std::string out;
    out += "\n    case $state in\n";
    out += "    (";
    out += cmd.name;
    out += ")\n";
    out += "        words=($line[";
    out += std::to_string(pos);
    out += "] \"${words[@]}\")\n";
    out += "        (( CURRENT += 1 ))\n";
    out += "        curcontext=\"${curcontext%:*:*}:";
    out += name_hyphen;
    out += "-command-$line[";
    out += std::to_string(pos);
    out += "]:\"\n";
    out += "        case $line[";
    out += std::to_string(pos);
    out += "] in\n";
    out += all_subcommands;
    out += "        esac\n";
    out += "    ;;\n";
    out += "    esac";
    return out;
}

/// Generate all `_{escaped_path}_commands()` helper functions.
/// One function per unique fn path (deduped by escaped path).
/// Root's own helper is included (from `flatten_subcommands(cmd, fn)`).
[[nodiscard]] std::string subcommand_details(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view fn)
{
    std::string out;

    // Root's own commands function (only if root has subcommands)
    if (cmd.has_subcommands()) {
        auto root_cmds = subcommands_of(cmd);
        if (!root_cmds.empty()) {
            out += "\n(( $+functions[_";
            out += fn;
            out += "_commands] )) ||\n_";
            out += fn;
            out += "_commands() {\n";
            out += "    local commands; commands=(\n";
            out += root_cmds;
            out += "    )\n";
            out += "    _describe -t commands '";
            out += cmd.name;
            out += " commands' commands \"$@\"\n";
            out += "}\n";
        }
    }

    // Subcommands at every level.  `flatten_subcommands` uses escaped
    // component names (callers pass fn=escape_name(root.name)), so
    // entry fn paths are already valid zsh identifiers.  We strip the
    // root prefix before calling `walk_command` (which uses raw names).
    auto entries = flatten_subcommands(cmd, fn);
    const std::string root_prefix = std::string(fn) + std::string(path_sep);

    // Dedup by fn path
    std::sort(entries.begin(), entries.end(),
        [](const SubCmdEntry& a, const SubCmdEntry& b) { return a.fn < b.fn; });
    entries.erase(
        std::unique(entries.begin(), entries.end(),
            [](const SubCmdEntry& a, const SubCmdEntry& b) { return a.fn == b.fn; }),
        entries.end());

    for (const auto& e : entries) {
        // Strip root prefix for tree navigation
        std::string_view sc_path = e.fn;
        if (sc_path.starts_with(root_prefix))
            sc_path.remove_prefix(root_prefix.size());

        auto target = walk_command(cmd, sc_path);
        if (!target || target->subcommands.empty()) continue;

        auto cmds = subcommands_of(*target);
        if (cmds.empty()) continue;

        out += "\n(( $+functions[_";
        out += e.fn;
        out += "_commands] )) ||\n_";
        out += e.fn;
        out += "_commands() {\n";
        out += "    local commands; commands=(\n";
        out += cmds;
        out += "    )\n";
        out += "    _describe -t commands '";
        out += cmd.name;
        out += " ";
        // Human-readable name: replace __subcmd__ with spaces
        {
            std::string human = e.fn;
            std::size_t pos = 0;
            while ((pos = human.find(path_sep, pos)) != std::string::npos) {
                human.replace(pos, path_sep.size(), " ");
                pos += 1;
            }
            out += human;
        }
        out += " commands' commands \"$@\"\n";
        out += "}\n";
    }

    return out;
}

} // unnamed namespace

namespace mcpplibs::cmdline::completions::zsh {

/// Generate a zsh completion script for the given command tree.
///
/// The output is a `#compdef` script containing a `_{name}()` function with
/// `_arguments` blocks and `case $state` dispatch, plus `_commands()`
/// helper functions for subcommand listing.
export void generate(const Command& cmd, std::ostream& out)
{
    using mcpplibs::cmdline::detail::Option;

    auto fn = escape_name(cmd.name);

    // Pre-collect root-level global options for re-offer under subcommands.
    std::vector<Option> root_globals;
    for (const auto& opt : cmd.options) {
        if (opt.global_) root_globals.emplace_back(opt);
    }

    auto initial_args = get_args_of(cmd, true, root_globals, fn);
    auto subcmds = get_subcommands_of(cmd, "", fn, root_globals);
    auto subcmd_dets = subcommand_details(cmd, fn);

    out << "#compdef " << cmd.name << "\n";
    out << "\n";
    out << "autoload -U is-at-least\n";
    out << "\n";
    out << "_" << fn << "() {\n";
    out << "    typeset -A opt_args\n";
    out << "    typeset -a _arguments_options\n";
    out << "    local ret=1\n";
    out << "\n";
    out << "    if is-at-least 5.2; then\n";
    out << "        _arguments_options=(-s -S -C)\n";
    out << "    else\n";
    out << "        _arguments_options=(-s -C)\n";
    out << "    fi\n";
    out << "\n";
    out << "    local context curcontext=\"$curcontext\" state line\n";
    out << initial_args << "\n";
    out << subcmds << "\n";
    out << "}\n";
    out << subcmd_dets << "\n";
    out << "if [ \"$funcstack[1]\" = \"_" << fn << "\" ]; then\n";
    out << "    _" << fn << " \"$@\"\n";
    out << "else\n";
    out << "    compdef _" << fn << " " << cmd.name << "\n";
    out << "fi\n";
}

} // namespace mcpplibs::cmdline::completions::zsh
