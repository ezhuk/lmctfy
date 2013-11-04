// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The lmctfy commandline app.

#include "lmctfy/cli/real_main.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
using ::std::string;
#include <memory>
#include <vector>

#include "base/callback.h"
#include "gflags/gflags.h"
#include "base/logging.h"
#include "base/walltime.h"
#include "lmctfy/cli/command.h"
#include "lmctfy/cli/commands/create.h"
#include "lmctfy/cli/commands/destroy.h"
#include "lmctfy/cli/commands/detect.h"
#include "lmctfy/cli/commands/enter.h"
#include "lmctfy/cli/commands/init.h"
#include "lmctfy/cli/commands/killall.h"
#include "lmctfy/cli/commands/list.h"
#include "lmctfy/cli/commands/notify.h"
#include "lmctfy/cli/commands/run.h"
#include "lmctfy/cli/commands/spec.h"
#include "lmctfy/cli/commands/stats.h"
#include "lmctfy/cli/output_map.h"
#include "include/lmctfy.h"
#include "strings/substitute.h"
#include "util/task/status.h"

using ::strings::Substitute;
using ::std::unique_ptr;
using ::std::vector;
using ::util::Status;

// Import some global flags.
DECLARE_bool(binarylog);
DECLARE_bool(lmctfy_binary);
DECLARE_bool(lmctfy_force);
DECLARE_bool(lmctfy_no_wait);
DECLARE_bool(lmctfy_recursive);
DECLARE_string(lmctfy_config);

// Define our app-specific command line flags.
// IMPORTANT: These flags are global across all linked components
// so make sure the flag names are unique!
DEFINE_string(lmctfy_output_style, "pairs",
              "Data output style: values, long, pairs");
DEFINE_bool(lmctfy_print_cmd_tree, false, "Print the command tree");
DEFINE_bool(lmctfy_print_cmd_tree_long, false,
            "Print the command tree (long)");
DEFINE_bool(lmctfy_print_help, false, "Print lmctfy help");
DEFINE_bool(lmctfy_version, false, "Print lmctfy version");
DEFINE_bool(lmctfy_version_long, false, "Print lmctfy version (long)");

namespace containers {
namespace lmctfy {
namespace cli {

// Gets the lmctfy version.
static const char *GetVersion() {
    return LMCTFY_VERSION;
}

// Gets information about when/how lmctfy was built.
static string GetBuildInfo() {
    return Substitute("built on $0 $1", __DATE__, __TIME__);
}

// Registers all supported commands.
static void RegisterCommands() {
  RegisterCreateCommand();
  RegisterDestroyCommand();
  RegisterDetectCommand();
  RegisterEnterCommand();
  RegisterInitCommand();
  RegisterKillAllCommand();
  RegisterListCommands();
  RegisterNotifyCommands();
  RegisterRunCommand();
  RegisterSpecCommand();
  RegisterStatsCommand();
}

static bool ParseShortFlags(int *argc, char ***argv) {
  int new_argc = 0;
  char ** new_argv = new char *[*argc];

  new_argv[new_argc++] = (*argv)[0];

  for (size_t i = 1; i < *argc; ++i) {
    char *cur_arg = (*argv)[i];

    if (cur_arg[0] != '-') {
      // Non-flag argument, keep it.
      new_argv[new_argc++] = cur_arg;
    } else if (strcmp(cur_arg, "-v") == 0) {
      FLAGS_lmctfy_version = true;
    } else if (strcmp(cur_arg, "-V") == 0) {
      FLAGS_lmctfy_version = true;
    } else if (strcmp(cur_arg, "-r") == 0) {
      FLAGS_lmctfy_recursive = true;
    } else if (strcmp(cur_arg, "-f") == 0) {
      FLAGS_lmctfy_force = true;
    } else if (strcmp(cur_arg, "-h") == 0) {
      FLAGS_lmctfy_print_help = true;
    } else if (strcmp(cur_arg, "-n") == 0) {
      FLAGS_lmctfy_no_wait = true;
    } else if (strcmp(cur_arg, "-b") == 0) {
      FLAGS_lmctfy_binary = true;
    } else if (strcmp(cur_arg, "-c") == 0) {
      // Fail if there is no other arg available.
      if (*argc - 1 == i) {
        fprintf(stderr, "Config file not specified with -c flag");
        return false;
      }

      // Copy the next value.
      ++i;
      FLAGS_lmctfy_config = (*argv)[i];
    } else {
      // Not a short flag, copy the argument.
      new_argv[new_argc++] = cur_arg;
    }
  }

  // Return new argc and argv.
  *argc = new_argc;
  *argv = new_argv;

  return true;
}

static int HandleCommand(const vector<string> &args_vector) {
  RegisterCommands();

  // Set the global OutputMap output style.
  OutputMap::Style output_style;
  if (FLAGS_lmctfy_output_style == "values") {
    output_style = OutputMap::STYLE_VALUES;
  } else if (FLAGS_lmctfy_output_style == "pairs") {
    output_style = OutputMap::STYLE_PAIRS;
  } else if (FLAGS_lmctfy_output_style == "long") {
    output_style = OutputMap::STYLE_LONG;
  } else {
    fprintf(stderr, "invalid style '%s': try 'values', 'long', or 'pairs'\n",
         FLAGS_lmctfy_output_style.c_str());
    return EXIT_FAILURE;
  }

  // Did the user ask for help?
  if (FLAGS_lmctfy_print_help) {
    PrintUsage(stdout, NULL);
    return EXIT_SUCCESS;
  }

  // Did the user ask for the command tree?
  if (FLAGS_lmctfy_print_cmd_tree_long || FLAGS_lmctfy_print_cmd_tree) {
    // Print out the tree in requested format.
    if (FLAGS_lmctfy_print_cmd_tree) {
      PrintCommandTree(stdout, NULL);
    } else if (FLAGS_lmctfy_print_cmd_tree_long) {
      PrintCommandTreeLong(stdout, NULL);
    }

    return EXIT_SUCCESS;
  }

  // Did the user ask for version info?
  if (FLAGS_lmctfy_version) {
    fprintf(stdout, "lmctfy version %s\n", GetVersion());
    return EXIT_SUCCESS;
  } else if (FLAGS_lmctfy_version_long) {
    fprintf(stdout, "lmctfy version %s %s\n", GetVersion(),
            GetBuildInfo().c_str());
    return EXIT_SUCCESS;
  }

  // Run the command.
  unique_ptr<ContainerApiFactory> lmctfy_factory(
      NewPermanentCallback(&ContainerApi::New));
  return RunCommand(args_vector, output_style, lmctfy_factory.get())
      .error_code();
}

// The main entry point from all forms of the binary.
int Main(int argc, char *argv[]) {
  WallTime time_at_start = WallTime_Now();

  // Do not log non-error messages to a file in the CLI at all by default.
  FLAGS_minloglevel = FLAGS_stderrthreshold;

  if (!ParseShortFlags(&argc, &argv)) {
    return EXIT_FAILURE;
  }

    ::google::ParseCommandLineFlags(&argc, &argv, true);

  // Execute command handling logic.
  vector<string> args_vector(argv, argv + argc);
  int ret = HandleCommand(args_vector);

  WallTime time_at_end = WallTime_Now();
  LOG(INFO) << "command completed in " << (time_at_end - time_at_start)
            << " seconds";

  return ret;
}

}  // namespace cli
}  // namespace lmctfy
}  // namespace containers
