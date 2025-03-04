// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides a simple class, |CommandLine|, for dealing with command lines (and
// flags and positional arguments).
//
// * Options (a.k.a. flags or switches) are all of the form "--name=<value>" (or
//   "--name", but this is indistinguishable from "--name="), where <value> is a
//   string. Not supported: "-name", "-n", "--name <value>", "-n <value>", etc.
// * Option order is preserved.
// * Option processing is stopped after the first positional argument[*]. Thus
//   in the command line "my_program --foo bar --baz", only "--foo" is an option
//   ("bar" and "--baz" are positional arguments).
// * Options can be looked up by name. If the same option occurs multiple times,
//   convention is to use the last occurrence (and the provided look-up
//   functions behave this way).
// * "--" may also be used to separate options from positional arguments. Thus
//   in the command line "my_program --foo -- --bar", "--bar" is a positional
//   argument.
// * |CommandLine|s store |argv[0]| and distinguish between not having |argv[0]|
//   and |argv[0]| being empty.
// * Apart from being copyable and movable, |CommandLine|s are immutable.
//
// There are factory functions to turn raw arguments into |CommandLine|s, in
// accordance with the above rules. However, |CommandLine|s may be used more
// generically (with the user transforming arguments using different rules,
// e.g., accepting "-name" as an option), subject to certain limitations (e.g.,
// not being able to distinguish "no value" from "empty value").
//
// [*] This is somewhat annoying for users, but: a. it's standard Unix behavior
// for most command line parsers, b. it makes "my_program *" (etc.) safer (which
// mostly explains a.), c. it makes parsing "subcommands", like "my_program
// --flag_for_my_program subcommand --flag_for_subcommand" saner.

#ifndef MOJO_EDK_UTIL_COMMAND_LINE_H_
#define MOJO_EDK_UTIL_COMMAND_LINE_H_

#include <cstddef>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace util {

// CommandLine -----------------------------------------------------------------

// Class that stores processed command lines ("argv[0]", options, and positional
// arguments) and provides access to them. For more details, see the file-level
// comment above. This class is thread-safe.
class CommandLine final {
 private:
  class ConstructionHelper;

 public:
  struct Option {
    Option() {}
    explicit Option(const std::string& name);
    Option(const std::string& name, const std::string& value);

    bool operator==(const Option& other) const {
      return name == other.name && value == other.value;
    }
    bool operator!=(const Option& other) const { return !operator==(other); }

    std::string name;
    std::string value;
  };

  // Default, copy, and move constructors (to be out-of-lined).
  CommandLine();
  CommandLine(const CommandLine& from);
  CommandLine(CommandLine&& from);

  // Constructs a |CommandLine| from its "components". This is especially useful
  // for creating a new |CommandLine| based on an existing |CommandLine| (e.g.,
  // adding options or arguments).
  explicit CommandLine(const std::string& argv0,
                       const std::vector<Option>& options,
                       const std::vector<std::string>& positional_args);

  ~CommandLine();

  // Copy and move assignment (to be out-of-lined).
  CommandLine& operator=(const CommandLine& from);
  CommandLine& operator=(CommandLine&& from);

  bool has_argv0() const { return has_argv0_; }
  const std::string& argv0() const { return argv0_; }
  const std::vector<Option> options() const { return options_; }
  const std::vector<std::string> positional_args() const {
    return positional_args_;
  }

  bool operator==(const CommandLine& other) const {
    // No need to compare |option_index_|.
    return has_argv0_ == other.has_argv0_ && argv0_ == other.argv0_ &&
           options_ == other.options_ &&
           positional_args_ == other.positional_args_;
  }
  bool operator!=(const CommandLine& other) const { return !operator==(other); }

  // Returns true if this command line has the option |name| (and if |index| is
  // non-null, sets |*index| to the index of the *last* occurrence of the given
  // option in |options()|) and false if not.
  bool HasOption(const std::string& name, size_t* index) const;

  // Gets the value of the option |name|. Returns true (and sets |*value|) on
  // success and false (leaving |*value| alone) on failure.
  bool GetOptionValue(const std::string& name, std::string* value) const;

  // Gets the value of the option |name|, with a default if the option is not
  // specified. (Note: This doesn't return a const reference, since this would
  // make the |default_value| argument inconvenient/dangerous.)
  std::string GetOptionValueWithDefault(const std::string& name,
                                        const std::string& default_value) const;

 private:
  bool has_argv0_ = false;
  // The following should all be empty if |has_argv0_| is false.
  std::string argv0_;
  std::vector<Option> options_;
  std::vector<std::string> positional_args_;

  // Maps option names to position in |options_|. If a given name occurs
  // multiple times, the index will be to the *last* occurrence.
  std::unordered_map<std::string, size_t> option_index_;

  // Allow copy and assignment.
};

// Factory functions (etc.) ----------------------------------------------------

namespace internal {

// Helper class for building command lines (finding options, etc.) from raw
// arguments.
class CommandLineBuilder final {
 public:
  CommandLineBuilder();
  ~CommandLineBuilder();

  // Processes an additional argument in the command line. Returns true if |arg|
  // is the *first* positional argument.
  bool ProcessArg(const std::string& arg);

  // Builds a |CommandLine| from the arguments processed so far.
  CommandLine Build() const;

 private:
  bool has_argv0_ = false;
  std::string argv0_;
  std::vector<CommandLine::Option> options_;
  std::vector<std::string> positional_args_;

  // True if we've started processing positional arguments.
  bool started_positional_args_ = false;

  MOJO_DISALLOW_COPY_AND_ASSIGN(CommandLineBuilder);
};

}  // namespace internal

// The following factory functions create |CommandLine|s from raw arguments in
// accordance with the rules outlined at the top of this file. (Other ways of
// transforming raw arguments into options and positional arguments are
// possible.)

// Like |CommandLineFromIterators()| (see below), but sets
// |*first_positional_arg| to point to the first positional argument seen (or
// |last| if none are seen). This is useful for processing "subcommands".
template <typename InputIterator>
inline CommandLine CommandLineFromIteratorsFindFirstPositionalArg(
    InputIterator first,
    InputIterator last,
    InputIterator* first_positional_arg) {
  if (first_positional_arg)
    *first_positional_arg = last;
  internal::CommandLineBuilder builder;
  for (auto it = first; it < last; ++it) {
    if (builder.ProcessArg(*it)) {
      if (first_positional_arg)
        *first_positional_arg = it;
    }
  }
  return builder.Build();
}

// Builds a |CommandLine| from first/last iterators (where |last| is really
// one-past-the-last, as usual) to |std::string|s or things that implicitly
// convert to |std::string|.
template <typename InputIterator>
inline CommandLine CommandLineFromIterators(InputIterator first,
                                            InputIterator last) {
  return CommandLineFromIteratorsFindFirstPositionalArg<InputIterator>(
      first, last, nullptr);
}

// Builds a |CommandLine| from the usual argc/argv.
inline CommandLine CommandLineFromArgcArgv(int argc, const char* const* argv) {
  return CommandLineFromIterators(argv, argv + argc);
}

// Builds a |CommandLine| from an initializer list of |std::string|s or things
// that implicitly convert to |std::string|.
template <typename StringType>
inline CommandLine CommandLineFromInitializerList(
    std::initializer_list<StringType> argv) {
  return CommandLineFromIterators(argv.begin(), argv.end());
}

// This is the "opposite" of the above factory functions, transforming a
// |CommandLine| into a vector of argument strings according to the rules
// outlined at the top of this file.
std::vector<std::string> CommandLineToArgv(const CommandLine& command_line);

}  // namespace util
}  // namespace mojo

#endif  // MOJO_EDK_UTIL_COMMAND_LINE_H_
