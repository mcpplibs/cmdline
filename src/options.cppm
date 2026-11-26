module;

export module mcpplibs.cmdline:options;

import std;

namespace mcpplibs::cmdline {

/// What a value names, for a shell that completes it: a file, a directory, or
/// nothing it can list.
export enum class ValueHint : std::uint8_t { none, file, directory };

/// The candidates for a value, given the text typed of it so far: what a
/// completion offers when the set is the program's to know (installed
/// packages, configured targets). Called only while a completion is asked for.
export using Candidates = std::function<std::vector<std::string>(std::string_view prefix)>;

} // namespace mcpplibs::cmdline

namespace mcpplibs::cmdline::detail {

// The facts a value carries for the parse and for a completion: the values it
// may take (a closed set the parse checks, which help names and a completion
// lists), what it names, and the program's candidates.
struct ValueFacts {
    std::vector<std::string> values_;
    ValueHint hint_ = ValueHint::none;
    Candidates complete_;

    [[nodiscard]] bool admits(std::string_view value) const {
        return values_.empty() || std::ranges::find(values_, value) != values_.end();
    }
    [[nodiscard]] std::string values_text() const {
        std::string out;
        for (const auto& v : values_) out += (out.empty() ? "" : ", ") + v;
        return out;
    }
};

export struct Arg : ValueFacts {
    std::string name;
    std::string help_;
    bool required_ = false;
    std::string default_val;
    bool repeatable_ = false;

    Arg(std::string_view name) : name(name) {}

    [[nodiscard]] Arg& help(std::string_view help_text) {
        help_ = help_text;
        return *this;
    }
    [[nodiscard]] Arg& required(bool required = true) {
        required_ = required;
        return *this;
    }
    [[nodiscard]] Arg& default_value(std::string_view value) {
        default_val = value;
        return *this;
    }
    /// The argument stands for every positional from here on (`mcpp add a b`).
    [[nodiscard]] Arg& repeatable(bool repeatable = true) {
        repeatable_ = repeatable;
        return *this;
    }
    /// The values it may take; any other is a parse error.
    [[nodiscard]] Arg& values(std::vector<std::string> allowed) {
        values_ = std::move(allowed);
        return *this;
    }
    [[nodiscard]] Arg& value_hint(ValueHint hint) {
        hint_ = hint;
        return *this;
    }
    [[nodiscard]] Arg& complete(Candidates candidates) {
        complete_ = std::move(candidates);
        return *this;
    }
};

export struct Option : ValueFacts {
    char short_ = '\0';
    std::string long_name;
    std::string help_;
    bool takes_value_ = false;
    bool multiple_ = false;
    bool global_ = false;
    std::string value_name_;
    // A value's check, declared with value_type<T>(): a value it refuses is a parse error.
    bool (*value_check_)(std::string_view) = nullptr;
    std::string value_kind_;  // what it takes, for that error: "a number", "an integer" …

    Option(std::string_view long_opt) : long_name(long_opt) {}
    Option(char short_char) : short_(short_char) {}

    /// The values it takes; any other is a parse error. The option takes a value.
    [[nodiscard]] Option& values(std::vector<std::string> allowed) {
        values_ = std::move(allowed);
        takes_value_ = true;
        return *this;
    }
    /// What its value names (a file, a directory). The option takes a value.
    [[nodiscard]] Option& value_hint(ValueHint hint) {
        hint_ = hint;
        takes_value_ = true;
        return *this;
    }
    /// The program's candidates for its value. The option takes a value.
    [[nodiscard]] Option& complete(Candidates candidates) {
        complete_ = std::move(candidates);
        takes_value_ = true;
        return *this;
    }

    [[nodiscard]] Option& short_name(char short_char) {
        short_ = short_char;
        return *this;
    }
    [[nodiscard]] Option& long_opt(std::string_view long_opt) {
        long_name = long_opt;
        return *this;
    }
    [[nodiscard]] Option& help(std::string_view help_text) {
        help_ = help_text;
        return *this;
    }
    [[nodiscard]] Option& takes_value(bool takes = true) {
        takes_value_ = takes;
        return *this;
    }
    [[nodiscard]] Option& multiple(bool multiple = true) {
        multiple_ = multiple;
        return *this;
    }
    [[nodiscard]] Option& global(bool global = true) {
        global_ = global;
        return *this;
    }
    [[nodiscard]] Option& value_name(std::string_view value_name) {
        value_name_ = value_name;
        return *this;
    }
    /// The option takes a number of type `Number`, read with std::from_chars:
    /// `--rate 2.5` with value_type<double>(), `--frames 60` with value_type<int>().
    /// A value that is not wholly one (`12px`, `-1` for an unsigned, out of
    /// range) is refused when the command line is parsed; afterwards
    /// ParsedArgs::value_as<Number>() reads it.
    template <class Number>
        requires(std::is_arithmetic_v<Number> && !std::is_same_v<Number, bool>)
    [[nodiscard]] Option& value_type() {
        takes_value_ = true;
        value_check_ = [](std::string_view text) {
            Number number{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
            return error == std::errc{} && end == text.data() + text.size();
        };
        value_kind_ = std::is_floating_point_v<Number> ? "a number"
                      : std::is_unsigned_v<Number>    ? "a whole number, not negative"
                                                      : "a whole number";
        if (value_name_.empty()) value_name_ = std::is_floating_point_v<Number> ? "NUMBER" : "N";
        return *this;
    }

    [[nodiscard]] bool matches_short(char short_char) const { return short_ && short_ == short_char; }
    [[nodiscard]] bool matches_long(std::string_view long_opt) const {
        return !long_name.empty() && long_name == long_opt;
    }
    [[nodiscard]] bool matches(std::string_view token) const {
        if (token.size() == 2 && token[0] == '-' && token[1] == short_) return true;
        if (token.size() > 2 && token.starts_with("--") && token.substr(2) == long_name) return true;
        return false;
    }
};

} // namespace mcpplibs::cmdline::detail
