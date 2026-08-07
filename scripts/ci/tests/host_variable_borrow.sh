#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "$0")/_lib.sh"

test_binary="${ROBOT_IPC_EXAMPLES_DIR}/host_variable_borrow/host_variable_borrow_test"

if [[ ! -x "${test_binary}" ]]; then
  skip_test "host_variable borrow example is not built"
fi

print_test "host_variable borrowed buffers"
cleanup_ipc_artifacts
"${test_binary}"
