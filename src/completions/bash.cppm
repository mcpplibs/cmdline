module;

export module mcpplibs.cmdline:completions.bash;

import std;
import :completions;

namespace {

/// Separator used to join subcommand names in the internal path representation
/// (e.g. `"myapp__subcmd__install__subcmd__config"`).
constexpr std::string_view path_sep = "__subcmd__";

/// Replace '-' with '__' so the result is a valid bash function name.
[[nodiscard]] std::string escape_name(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) result += (c == '-') ? "__" : std::string(1, c);
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

/// An entry in the flattened subcommand tree: the function-name path of the
/// parent, the subcommand's display name, and the subcommand's own
/// function-name path.
struct SubCmdEntry {
    std::string parent_fn;
    std::string name;
    std::string fn;
};

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

/// Append the short and/or long representation of one option to the word
/// list string.
void append_option(std::string& opts, const mcpplibs::cmdline::detail::Option& opt) {
    if (opt.short_) { opts += " -"; opts += opt.short_; }
    if (!opt.long_name.empty()) { opts += " --"; opts += opt.long_name; }
}

/// Append root-level globals (deduped) to the word list.
void append_root_globals(
    std::string& opts,
    std::span<const mcpplibs::cmdline::detail::Option> root_globals,
    const mcpplibs::cmdline::completions::Command& target)
{
    for (const auto& rg : root_globals) {
        if (has_option(target, rg.short_, rg.long_name)) continue;
        append_option(opts, rg);
    }
}

/// Build the space-separated word list for `compgen -W` at a given path.
/// Includes local options, subcommand names, and (when path is non-empty)
/// root-level globals.
[[nodiscard]] std::string all_options_for_path(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view path,
    std::span<const mcpplibs::cmdline::detail::Option> root_globals)
{
    const auto* target = walk_command(cmd, path);
    if (!target) return {};

    std::string opts;

    for (const auto& opt : target->options) append_option(opts, opt);

    for (const auto& sub : target->subcommands) {
        opts += ' ';
        opts += sub.name;
    }

    if (!path.empty()) append_root_globals(opts, root_globals, *target);

    if (!opts.empty() && opts[0] == ' ') opts.erase(0, 1);
    return opts;
}

/// Write one `case "$prev" in` pattern for a value-taking option.
/// Uses `compgen -f` for file completion, matching the stripped-down
/// feature set of our fish generator (no possible-values, no value-hints).
void append_option_detail(
    std::string& out,
    const mcpplibs::cmdline::detail::Option& opt)
{
    std::string patterns;
    if (opt.short_) { patterns += '-'; patterns += opt.short_; }
    if (!opt.long_name.empty()) {
        if (!patterns.empty()) patterns += '|';
        patterns += "--";
        patterns += opt.long_name;
    }
    if (patterns.empty()) return;

    out += "                ";
    out += patterns;
    out += ")\n";
    out += "                    COMPREPLY=($(compgen -f \"${cur}\"))\n";
    out += "                    return 0\n";
    out += "                    ;;\n";
}

/// Build the `case "$prev" in` block for value-taking options at a given
/// path.  Includes root-level globals when path is non-empty.
[[nodiscard]] std::string option_details_for_path(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view path,
    std::span<const mcpplibs::cmdline::detail::Option> root_globals)
{
    const auto* target = walk_command(cmd, path);
    if (!target) return {};

    std::string out;

    for (const auto& opt : target->options) {
        if (opt.takes_value_) append_option_detail(out, opt);
    }

    if (!path.empty()) {
        for (const auto& rg : root_globals) {
            if (!rg.takes_value_) continue;
            if (has_option(*target, rg.short_, rg.long_name)) continue;
            append_option_detail(out, rg);
        }
    }

    return out;
}

/// Build the subcommand-detection cases for the prologue loop that walks
/// `COMP_WORDS` to determine which subcommand the user is in.
[[nodiscard]] std::string subcommand_detection_cases(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::string_view fn)
{
    const auto entries = flatten_subcommands(cmd, fn);
    if (entries.empty()) return {};

    std::string out;
    for (const auto& e : entries) {
        out += "            \"";
        out += e.parent_fn;
        out += ",";
        out += e.name;
        out += "\")\n";
        out += "                cmd=\"";
        out += e.fn;
        out += "\"\n";
        out += "                ;;\n";
    }
    return out;
}

/// Build the `case "${cmd}" in` dispatch branches for every unique
/// subcommand path.  `fn` is the escaped root name, used as the parent
/// prefix for flattened paths so that the case labels match the values
/// set by the prologue loop.
/// Branches are deduplicated by `fn` so that future alias support does
/// not produce duplicate `case` labels.
[[nodiscard]] std::string subcommand_details(
    const mcpplibs::cmdline::completions::Command& cmd,
    std::span<const mcpplibs::cmdline::detail::Option> root_globals,
    std::string_view fn)
{
    const auto entries = flatten_subcommands(cmd, fn);

    // Paths from flatten_subcommands include the root fn prefix
    // (e.g. "myapp__subcmd__install"), but walk_command needs only the
    // subcommand components (e.g. "install").
    const std::string root_prefix = std::string(fn) + std::string(path_sep);

    struct ScBranch {
        std::string fn;
        std::string path; // path_sep-separated relative to root
        int depth;
    };
    std::vector<ScBranch> branches;
    for (const auto& e : entries) {
        std::string_view sc_path = e.fn;
        if (sc_path.starts_with(root_prefix))
            sc_path.remove_prefix(root_prefix.size());

        // Depth: number of __subcmd__ separators in the relative path + 1
        int depth = 1;
        for (std::size_t pos = 0;
             (pos = sc_path.find(path_sep, pos)) != std::string::npos;
             pos += path_sep.size())
            ++depth;
        branches.push_back({e.fn, std::string(sc_path), depth});
    }

    if (branches.empty()) return {};

    // Sort and dedup by fn so that the output never produces duplicate
    // `case` labels even if future alias expansion creates identical paths.
    std::sort(branches.begin(), branches.end(),
        [](const ScBranch& a, const ScBranch& b) { return a.fn < b.fn; });
    branches.erase(
        std::unique(branches.begin(), branches.end(),
            [](const ScBranch& a, const ScBranch& b) { return a.fn == b.fn; }),
        branches.end());

    std::string out;
    for (const auto& br : branches) {
        auto opts = all_options_for_path(cmd, br.path, root_globals);
        auto opts_details = option_details_for_path(cmd, br.path, root_globals);

        // COMP_CWORD level = depth of this subcommand + 1 (the binary name itself)
        int level = br.depth + 1;

        out += "        ";
        out += br.fn;
        out += ")\n";
        out += "            opts=\"";
        out += opts;
        out += "\"\n";
        out += "            if [[ ${cur} == -* || ${COMP_CWORD} -eq ";
        out += std::to_string(level);
        out += " ]] ; then\n";
        out += "                COMPREPLY=($(compgen -W \"${opts}\" -- \"${cur}\"))\n";
        out += "                return 0\n";
        out += "            fi\n";
        out += "            case \"${prev}\" in\n";
        out += opts_details;
        out += "                *)\n";
        out += "                    COMPREPLY=()\n";
        out += "                    ;;\n";
        out += "            esac\n";
        out += "            COMPREPLY=($(compgen -W \"${opts}\" -- \"${cur}\"))\n";
        out += "            return 0\n";
        out += "            ;;\n";
    }
    return out;
}

} // unnamed namespace

namespace mcpplibs::cmdline::completions::bash {

/// Generate a bash completion script for the given command tree.
///
/// The output is a single `_{name}()` function registered with `complete -F`
/// that uses `compgen -W` for word-list matching and a prologue `case` loop
/// to detect which subcommand the user is currently typing.
export void generate(const Command& cmd, std::ostream& out)
{
    auto fn = escape_name(cmd.name);
    auto name_opts = all_options_for_path(cmd, "", {});
    auto name_opts_details = option_details_for_path(cmd, "", {});
    auto subcmds_cases = subcommand_detection_cases(cmd, fn);

    // Pre-collect root-level global options so subcommand helpers can re-offer
    // them (same pattern as fish.cppm's `gen_inner` global re-offer block).
    std::vector<detail::Option> root_globals;
    for (const auto& opt : cmd.options) {
        if (opt.global_) root_globals.emplace_back(opt);
    }
    auto subcmd_detail = subcommand_details(cmd, root_globals, fn);

    // The bash completion function follows clap_complete's layout: a prologue
    // that sets up `cur`/`prev`/`cmd`, a loop to detect the current
    // subcommand path, and a `case "${cmd}" in` switch with one branch per
    // valid path (root + each subcommand).

    out << "_" << fn << "() {\n";
    out << "    local i cur prev opts cmd\n";
    out << "    COMPREPLY=()\n";
    out << "    if [[ \"${BASH_VERSINFO[0]}\" -ge 4 ]]; then\n";
    out << "        cur=\"$2\"\n";
    out << "    else\n";
    out << "        cur=\"${COMP_WORDS[COMP_CWORD]}\"\n";
    out << "    fi\n";
    out << "    prev=\"$3\"\n";
    out << "    cmd=\"\"\n";
    out << "    opts=\"\"\n";
    out << "\n";
    out << "    for i in \"${COMP_WORDS[@]:0:COMP_CWORD}\"\n";
    out << "    do\n";
    out << "        case \"${cmd},${i}\" in\n";
    out << "            \"," << cmd.name << "\")\n";
    out << "                cmd=\"" << fn << "\"\n";
    out << "                ;;\n";
    out << subcmds_cases;
    out << "            *)\n";
    out << "                ;;\n";
    out << "        esac\n";
    out << "    done\n";
    out << "\n";
    out << "    case \"${cmd}\" in\n";
    out << "        " << fn << ")\n";
    out << "            opts=\"" << name_opts << "\"\n";
    out << "            if [[ ${cur} == -* || ${COMP_CWORD} -eq 1 ]] ; then\n";
    out << "                COMPREPLY=($(compgen -W \"${opts}\" -- \"${cur}\"))\n";
    out << "                return 0\n";
    out << "            fi\n";
    out << "            case \"${prev}\" in\n";
    out << name_opts_details;
    out << "                *)\n";
    out << "                    COMPREPLY=()\n";
    out << "                    ;;\n";
    out << "            esac\n";
    out << "            COMPREPLY=($(compgen -W \"${opts}\" -- \"${cur}\"))\n";
    out << "            return 0\n";
    out << "            ;;\n";
    out << subcmd_detail;
    out << "    esac\n";
    out << "}\n";
    out << "\n";
    out << "if [[ \"${BASH_VERSINFO[0]}\" -eq 4 && \"${BASH_VERSINFO[1]}\" -ge 4 || \"${BASH_VERSINFO[0]}\" -gt 4 ]]; then\n";
    out << "    complete -F _" << fn << " -o nosort -o bashdefault -o default " << cmd.name << "\n";
    out << "else\n";
    out << "    complete -F _" << fn << " -o bashdefault -o default " << cmd.name << "\n";
    out << "fi\n";
}

}; // namespace mcpplibs::cmdline::completions::bash
