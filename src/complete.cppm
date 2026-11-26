module;

export module mcpplibs.cmdline:complete;

import std;
import :options;
import :completions;

// THE COMPLETION OF A COMMAND LINE, ONE ENGINE FOR EVERY SHELL.
//
// A shell asks the program: these are the words before the cursor, the last of
// them the one being typed; what may it become? The program answers from its
// own command tree, read by the rules its parse reads it by (`classify` below
// is what both call), so a completion never offers what the parse refuses and
// never misses what it takes. A shell's part is an adapter of a few lines that
// passes the words and lists the answer (`adapter`), the same for every
// program: nothing of the tree is copied into a script, and a program that
// gains a command completes it without its users installing anything again.
//
// The answer is text, one candidate per line (`value<TAB>description`), and a
// last line that says what the shell should add itself:
//
//   :0   nothing          the candidates are the whole answer
//   :1   files            the value names a file
//   :2   directories      the value names a directory

namespace mcpplibs::cmdline::completions {

export enum class Directive : std::uint8_t { none = 0, files = 1, directories = 2 };

export struct Candidate {
    std::string value;
    std::string description;
};

export struct Answer {
    std::vector<Candidate> candidates;
    Directive directive = Directive::none;
};

// What a word of a command line is to a command: the rules the parse reads it
// by. `--` ends the options; `--name` and `--name=value` name a long option;
// `-c` a short one; a subcommand's name a subcommand; anything else is a
// positional.
export struct Classified {
    enum Kind : std::uint8_t { end_of_options, long_option, short_option, subcommand, positional };
    Kind kind = positional;
    std::string_view key;          // the option's name, or the subcommand's
    std::optional<std::string_view> inline_value;   // `--name=value`
};

export [[nodiscard]] Classified classify(std::string_view token, bool has_subcommand_named) {
    if (token == "--") return {Classified::end_of_options, {}, {}};
    if (token.starts_with("--") && token.size() > 2) {
        auto rest = token.substr(2);
        auto eq = rest.find('=');
        if (eq == std::string_view::npos) return {Classified::long_option, rest, {}};
        return {Classified::long_option, rest.substr(0, eq), rest.substr(eq + 1)};
    }
    if (token.size() == 2 && token[0] == '-' && token[1] != '-')
        return {Classified::short_option, token.substr(1), {}};
    if (has_subcommand_named) return {Classified::subcommand, token, {}};
    return {Classified::positional, token, {}};
}

namespace detail {

std::string escape(std::string_view s) {
    std::string out;
    for (char c : s) out += (c == '\t' || c == '\n') ? ' ' : c;
    return out;
}

void offer(Answer& a, std::string_view value, std::string_view prefix, std::string_view description = {}) {
    if (value.starts_with(prefix)) a.candidates.push_back({std::string(value), escape(description)});
}

// The value's candidates: its closed set, the program's, and what it names.
void offer_value(Answer& a, const mcpplibs::cmdline::detail::ValueFacts& facts, std::string_view prefix,
                 std::string_view keep = {}) {
    for (const auto& v : facts.values_) offer(a, std::string(keep) + v, std::string(keep) + std::string(prefix));
    if (facts.complete_)
        for (const auto& v : facts.complete_(prefix)) offer(a, std::string(keep) + v, std::string(keep) + std::string(prefix));
    if (facts.values_.empty() && !facts.complete_)
        a.directive = facts.hint_ == ValueHint::directory ? Directive::directories
                    : facts.hint_ == ValueHint::file      ? Directive::files
                                                          : Directive::none;
}

const Command* find_sub(const Command& cmd, std::string_view name) {
    for (const auto& sub : cmd.subcommands)
        if (sub.name == name) return &sub;
    return nullptr;
}

const mcpplibs::cmdline::detail::Option* find_option(const Command& cmd, const std::vector<const mcpplibs::cmdline::detail::Option*>& globals,
                                                     std::string_view key, bool is_short) {
    auto matches = [&](const mcpplibs::cmdline::detail::Option& o) {
        return is_short ? (key.size() == 1 && o.matches_short(key[0])) : o.matches_long(key);
    };
    for (const auto& o : cmd.options)
        if (matches(o)) return &o;
    for (const auto* o : globals)
        if (matches(*o)) return o;
    return nullptr;
}

} // namespace detail

/// What the word being typed may become. `words` are the words after the
/// program's name; the last is the one being typed, empty when the cursor
/// stands after a space. A pure function of the tree and the words: a
/// candidate callback (`complete`) is the program's, and runs only here.
export [[nodiscard]] Answer complete(const Command& root, std::span<const std::string> words) {
    Answer answer;
    const Command* cmd = &root;
    std::vector<const mcpplibs::cmdline::detail::Option*> globals;
    for (const auto& o : root.options)
        if (o.global_) globals.push_back(&o);
    std::size_t positional = 0;
    bool only_positionals = false;
    const mcpplibs::cmdline::detail::Option* pending = nullptr;   // an option waiting for its value
    const std::string current = words.empty() ? std::string{} : words.back();
    for (std::size_t i = 0; i + 1 < words.size(); ++i) {
        const std::string_view w = words[i];
        if (pending) { pending = nullptr; continue; }
        if (only_positionals) { ++positional; continue; }
        const auto c = classify(w, !w.starts_with('-') && detail::find_sub(*cmd, w));
        switch (c.kind) {
            case Classified::end_of_options: only_positionals = true; break;
            case Classified::long_option:
            case Classified::short_option: {
                const auto* o = detail::find_option(*cmd, globals, c.key, c.kind == Classified::short_option);
                if (o && o->takes_value_ && !c.inline_value) pending = o;
                break;
            }
            case Classified::subcommand: {
                cmd = detail::find_sub(*cmd, w);
                positional = 0;
                for (const auto& o : cmd->options)
                    if (o.global_) globals.push_back(&o);
                break;
            }
            case Classified::positional: ++positional; break;
        }
    }
    if (pending) {
        detail::offer_value(answer, *pending, current);
        return answer;
    }
    if (!only_positionals && current.starts_with("--") && current.find('=') != std::string::npos) {
        const auto c = classify(current, false);
        if (const auto* o = detail::find_option(*cmd, globals, c.key, false); o && o->takes_value_)
            detail::offer_value(answer, *o, c.inline_value.value_or(""), std::format("--{}=", c.key));
        return answer;
    }
    if (!only_positionals && current.starts_with('-')) {
        auto list = [&](const mcpplibs::cmdline::detail::Option& o) {
            if (!o.long_name.empty()) detail::offer(answer, "--" + o.long_name, current, o.help_);
            else if (o.short_) detail::offer(answer, std::string{'-', o.short_}, current, o.help_);
        };
        for (const auto& o : cmd->options) list(o);
        for (const auto* o : globals)
            if (std::ranges::none_of(cmd->options, [&](const auto& own) {
                    return &own == o || (!own.long_name.empty() && own.long_name == o->long_name);
                }))
                list(*o);
        return answer;
    }
    if (!only_positionals)
        for (const auto& sub : cmd->subcommands) detail::offer(answer, sub.name, current, sub.description);
    const mcpplibs::cmdline::detail::Arg* arg = nullptr;
    if (positional < cmd->args.size()) arg = &cmd->args[positional];
    else if (!cmd->args.empty() && cmd->args.back().repeatable_) arg = &cmd->args.back();
    if (arg) {
        const auto before = answer.candidates.size();
        detail::offer_value(answer, *arg, current);
        // Subcommands offered beside a value that names a file: the shell lists both.
        if (answer.candidates.size() > before) answer.directive = Directive::none;
    }
    return answer;
}

/// The answer as the adapters read it.
export [[nodiscard]] std::string render(const Answer& answer) {
    std::string out;
    for (const auto& c : answer.candidates)
        out += c.description.empty() ? c.value + "\n" : std::format("{}\t{}\n", c.value, c.description);
    out += std::format(":{}\n", static_cast<int>(answer.directive));
    return out;
}

/// The script a shell reads to complete `program`: it passes the words to
/// `program <entry>` and lists what comes back. Nothing of the command tree is
/// in it.
export [[nodiscard]] std::string adapter(Shell shell, std::string_view program, std::string_view entry) {
    std::string fn;
    for (char c : program) fn += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    switch (shell) {
        case Shell::bash:
            return std::format(R"(# bash completion for {0}: the program answers (`{0} {1} <words>`).
_{2}_complete() {{
    local IFS=$'\n' line directive
    local -a out
    out=($("{0}" {1} "${{COMP_WORDS[@]:1:COMP_CWORD}}" 2>/dev/null))
    directive="${{out[-1]#:}}"
    unset 'out[-1]'
    COMPREPLY=()
    for line in "${{out[@]}}"; do COMPREPLY+=("${{line%%$'\t'*}}"); done
    if [[ ${{#COMPREPLY[@]}} -eq 0 ]]; then
        case "$directive" in
            1) COMPREPLY=($(compgen -f -- "${{COMP_WORDS[COMP_CWORD]}}")) ;;
            2) COMPREPLY=($(compgen -d -- "${{COMP_WORDS[COMP_CWORD]}}")) ;;
        esac
    fi
}}
complete -o default -F _{2}_complete {0}
)", program, entry, fn);
        case Shell::zsh:
            return std::format(R"(#compdef {0}
# zsh completion for {0}: the program answers (`{0} {1} <words>`).
_{2}_complete() {{
    local -a out values
    local line directive
    out=("${{(@f)$("{0}" {1} "${{(@)words[2,CURRENT]}}" 2>/dev/null)}}")
    directive="${{out[-1]#:}}"
    out=("${{(@)out[1,-2]}}")
    for line in "${{out[@]}}"; do values+=("${{line//:/\\:}}"); done
    values=("${{(@)values//$'\t'/:}}")
    if (( ${{#values}} )); then
        _describe -t values '{0}' values
    elif [[ "$directive" == 1 ]]; then
        _files
    elif [[ "$directive" == 2 ]]; then
        _files -/
    fi
}}
compdef _{2}_complete {0}
)", program, entry, fn);
        case Shell::fish:
            return std::format(R"(# fish completion for {0}: the program answers (`{0} {1} <words>`).
function __{2}_complete
    set -l words (commandline -opc)[2..-1] (commandline -ct)
    set -l out ("{0}" {1} $words 2>/dev/null)
    set -l directive (string replace ':' '' -- $out[-1])
    set -e out[-1]
    if test (count $out) -gt 0
        printf '%s\n' $out
    else if test "$directive" = 1
        __fish_complete_path (commandline -ct)
    else if test "$directive" = 2
        __fish_complete_directories (commandline -ct)
    end
end
complete -c {0} -f -a '(__{2}_complete)'
)", program, entry, fn);
        case Shell::powershell:
            return std::format(R"(# PowerShell completion for {0}: the program answers (`{0} {1} <words>`).
Register-ArgumentCompleter -Native -CommandName '{0}' -ScriptBlock {{
    param($wordToComplete, $commandAst, $cursorPosition)
    $words = @($commandAst.CommandElements | Select-Object -Skip 1 | ForEach-Object {{ $_.ToString() }})
    if ($wordToComplete -eq '') {{ $words += '' }}
    $out = @(& '{0}' {1} @words 2>$null)
    if ($out.Count -eq 0) {{ return }}
    $directive = $out[-1].TrimStart(':')
    $lines = if ($out.Count -gt 1) {{ $out[0..($out.Count - 2)] }} else {{ @() }}
    foreach ($line in $lines) {{
        $value, $help = $line -split "`t", 2
        if (-not $help) {{ $help = $value }}
        [System.Management.Automation.CompletionResult]::new($value, $value, 'ParameterValue', $help)
    }}
    if ($lines.Count -eq 0 -and $directive -ne '0') {{
        Get-ChildItem -Path "$wordToComplete*" -Directory:($directive -eq '2') |
            ForEach-Object {{ [System.Management.Automation.CompletionResult]::new($_.Name, $_.Name, 'ProviderItem', $_.Name) }}
    }}
}}
)", program, entry);
    }
    return {};
}

} // namespace mcpplibs::cmdline::completions
