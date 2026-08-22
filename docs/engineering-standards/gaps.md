# Engineering standards - audit findings

Audited: 2026-08-22. See `applied.md` for selected groups.

## Closed 2026-08-22

1. **G2 Coverage enforcement** - `codecov/codecov-action` now `fail_ci_if_error: true`;
   `.codecov.yml` has a `patch:` status at 80% (diff-gated, per selection). Status: closed.

2. **G3 CI/local parity** - CI `lint` job now runs `cppcheck` (matching pre-commit's args) and
   `clang-tidy` (`.clang-tidy` was present but unused; now run via a `compile_commands.json`
   configure step). Status: closed.

3. **Rule 27 (mandatory) - action pinning** - all `uses:` steps in `ci.yml` pin to a commit SHA
   with the version as a trailing comment. Status: closed.

4. **Rule 27 (mandatory) - concurrency** - `release` job has
   `concurrency: {group: release-${{ github.ref }}, cancel-in-progress: false}`. Status: closed.

5. **Rule 18 - dependency vulnerability scanning** - new `dep-vuln-scan` CI job runs
   `scripts/check_dep_vulns.ps1`: resolves each `FetchContent_Declare` git-tag pin to a commit,
   queries the OSV.dev public commit-lookup API, fails on any known advisory. Runs on every
   push/PR to `main` plus a Monday 06:00 UTC schedule (rule 18's "scanning on add/bump alone is
   insufficient" clause). Reviewed by an independent agent; regex confirmed to correctly match
   all four `FetchContent_Declare` blocks (googletest, absl, re2, benchmark). Status: closed.

6. **G5 Logging** - `Logger::Log` now takes a `LogLevel` (Critical/Error/Warning/Info/Debug/
   Verbose per rule 17's hierarchy), filters against a runtime-settable threshold
   (`Logger::SetLevel`), and writes structured `level=X msg="..."` lines. `Settings` gained
   `GetLogLevel()`/`SetLogLevel()` backed by an INI key (`[General] LogLevel`, default WARNING,
   falls back to WARNING on an unparseable value). 3 call sites in `MainWindow.cpp` updated with
   explicit levels. 7 tests added (`tests/test_Logger.cpp`, `tests/test_Settings.cpp`) per the
   agreed test-case checkpoint. Status: closed.

7. **G6 architecture.md missing** - `docs/architecture.md` added: component map, layout,
   concurrency model, external dependencies, end-to-end data flow. Status: closed.

8. **G6/rule 28 - release integrity (checksums/docs part)** - release job now generates
   `SHA256SUMS.txt` for ZIP/NSIS/exe assets and uploads it alongside them; `README.md` documents
   verification. Status: closed.
   **Code-signing part - deferred, developer action required.** Rule 28 says check free signing
   paths before accepting unsigned. Windows code-signing via SignPath Foundation requires an
   organization application, approval, and a GitHub secret - outside what an agent can do
   unilaterally. **Action item for the developer:** apply at
   https://signpath.org/apply for OSS code signing if pursued; until then, checksums (now in
   place) remain the documented verification path, stated accurately in `README.md`.

9. **G8 Instrumentation** - new `benchmarks/` target (Google Benchmark, FetchContent-added
   behind `BUILD_BENCHMARKS` option, default OFF) benchmarks `IndexPool::AddEntry` (index-build
   throughput) and `SearchEngine::Search` (substring + regex modes). New `benchmark` CI job
   builds it, runs with `--benchmark_out_format=json`, accumulates history via `actions/cache`,
   and renders a trend table into the job summary via `scripts/render_benchmark_trend.py`.
   Status: closed.

## Verification status

**Not compiled or run.** This repo is Windows-only (`CMakeLists.txt`:
`if(NOT WIN32) message(FATAL_ERROR ...)`) and was audited/fixed from a Linux environment with no
MSVC toolchain available. Every change above was reviewed by an independent fresh-context agent
(`caveman:cavecrew-reviewer`) reading the diff against the real header signatures it touches;
2 low-severity findings from that review were fixed (UTF-8-unsafe test file reads in
`test_Logger.cpp`; a benchmark-result filename collision risk in the `benchmark` CI job). No
build, no `ctest`, no `pre-commit run --all-files` has actually been executed against this
change. **Run the full toolchain on Windows (or push and watch CI) before trusting this green**;
rule 16's gate-verification requirement (`applied.md`) still needs to happen for real once that's
possible.

## Checked, no gap

- **G1 Testing discipline** - suite exists (GoogleTest + mocks); TDD/bug-fix-proof workflow
  applies going forward, not a repo artifact to fix.
- **G7 Interfaces and design** - volatile-dependency seams already correct:
  `IFileSystemScanner`, `IIndexStore`, `IUsnJournalMonitor`, `ISearchEngine`, each with a real
  mock (`tests/mocks/`). No action needed.
- **Rule 26 (badges)** - single `ci.yml` workflow, README badge present and correctly linked.
- **ADRs (rule 1)** - six present in `docs/adr/`, correct `Status:` line format observed on
  sampled entry.
