#!/usr/bin/env bash
# Runeforge workstation policy checks (host build): the shared reference-model vectors
# (tests/data/workstation-vectors.v3.txt from feasr-core/alpha/workstation/model/gen_vectors_v3.mjs, model v4; RC1-WS r2, R3.1)
# plus the input, protocol v3, context/legacy, layout, shot, lease, controls, seat, staging gate, readiness, view and
# free-camera policy unit checks. RC1's workstation-vectors.v1.txt and RC1-WS r1's workstation-vectors.v2.txt are kept as
# history and their freshness is still checked.
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

# R8.7.4.6: never report a silent PASS. The production CTest runs this under WSL, where `c++` may be absent; earlier runs
# printed "skipped" and exited 0, so the workstation assertions did not execute. Find a C++23 host compiler or fail.
cxx="${CXX:-}"
if [[ -z "${cxx}" ]]; then
  for candidate in c++ clang++ clang++-19 /usr/lib/llvm-19/bin/clang++ g++; do
    if command -v "${candidate}" >/dev/null 2>&1; then
      cxx="${candidate}"
      break
    fi
  done
fi
if [[ -z "${cxx}" ]] || ! command -v "${cxx}" >/dev/null 2>&1; then
  if [[ "${RUNEFORGE_WORKSTATION_ALLOW_SKIP:-0}" == "1" ]]; then
    echo "runeforge_workstation_checks: SKIPPED by RUNEFORGE_WORKSTATION_ALLOW_SKIP=1 (no host C++ compiler)"
    exit 0
  fi
  echo "runeforge_workstation_checks: FAIL (no host C++ compiler; set CXX)" >&2
  exit 1
fi
echo "runeforge_workstation_checks: compiler ${cxx}"

"${cxx}" -std=c++23 -O2 -Wall -Wextra -pedantic \
  -I"${project_dir}/include" \
  -I"${project_dir}/extern/vendor/include" \
  "${script_dir}/runeforge_workstation_policy_tests.cpp" \
  -o "${tmp_dir}/runeforge_workstation_policy_tests"
"${tmp_dir}/runeforge_workstation_policy_tests" "${script_dir}/data/workstation-vectors.v3.txt" \
  "${script_dir}/data/runeforge-snapshot-shape.v1.json"

# R8.7.4.14: inscription reader input/exit contract (live U1 soft-lock).
"${cxx}" -std=c++23 -O2 -Wall -Wextra -pedantic \
  -I"${project_dir}/include" \
  "${script_dir}/runeforge_reader_contract_tests.cpp" \
  -o "${tmp_dir}/runeforge_reader_contract_tests"
"${tmp_dir}/runeforge_reader_contract_tests"
if command -v node >/dev/null 2>&1; then
  node "${script_dir}/runeforge_reader_ui.test.cjs"
elif command -v node.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1; then
  node.exe "$(wslpath -w "${script_dir}/runeforge_reader_ui.test.cjs")"
else
  echo "runeforge_workstation_checks: FAIL (node is required for the reader view contract test)" >&2
  exit 1
fi

if command -v python3 >/dev/null 2>&1; then
  python3 "${script_dir}/runeforge_back_input_static_test.py" --source-root "${project_dir}"
  python3 "${script_dir}/runeforge_snapshot_contract_static_test.py" --source-root "${project_dir}"
  python3 "${script_dir}/runeforge_reader_input_static_test.py" --source-root "${project_dir}"
  python3 "${script_dir}/runeforge_presentation_state_static_test.py" --source-root "${project_dir}" --core-root "${project_dir}/../../../feasr-core"
  # R8.7.4.21 I3: RunewordEffectCard v1 wiring (T4/T5/T7 source) and the all-recipe generator (T3, repo affixes.json)
  python3 "${script_dir}/runeforge_effect_card_static_test.py" --source-root "${project_dir}"
  python3 "${script_dir}/runeword_effect_card_catalog_check.py" --cxx "${cxx}"
else
  echo "runeforge_workstation_checks: skipped Back-input source wiring test (no python3)"
fi

# The vectors are generated copies; when the feasr-core sibling is present, prove the copies are current.
core_model_dir="${project_dir}/../../../feasr-core/alpha/workstation/model"
if [[ -f "${core_model_dir}/gen_vectors_v3.mjs" ]] && command -v node >/dev/null 2>&1; then
  node "${core_model_dir}/gen_vectors_v3.mjs" --check --native-out "${script_dir}/data/workstation-vectors.v3.txt"
  node "${core_model_dir}/gen_vectors_v2.mjs" --check --native-out "${script_dir}/data/workstation-vectors.v2.txt"
  node "${core_model_dir}/gen_vectors.mjs" --check --native-out "${script_dir}/data/workstation-vectors.v1.txt"
else
  echo "runeforge_workstation_checks: vector freshness not checked (feasr-core sibling or node unavailable)"
fi
