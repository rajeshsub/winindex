# Engineering standards - applied selection

Recorded: 2026-08-22
Posture: Greenfield (own project)

## Groups selected

- G1 Testing discipline - selected
- G2 Coverage enforcement - selected (gating model: diff-gated)
- G3 Toolchain, gates and CI - selected (commit gate split: fast at pre-commit, full at pre-push - matches current `.pre-commit-config.yaml`)
- G5 Logging and observability - selected (depth: full structured records)
- G6 Docs, ADRs, architecture and README - selected
- G7 Interfaces and design - selected
- G8 Instrumentation and performance - selected (scope: test suite plus latency-sensitive paths)

Testing tiers (G1): unit + integration meaningful (GoogleTest + mock seams already present:
`IFileSystemScanner`, `IIndexStore`, `IUsnJournalMonitor`, `ISearchEngine`). E2E and contract
tiers not meaningful for this desktop app. Performance tier folds into G8.

## Not offered (always bind)

Rule 0 (no fabricated claims), no security holes, no committed credentials, credential
hashing/data classification/encryption (rules 7-8), no secret/PII log leakage (rule 17),
verified backup before destructive migration, AI never commits, independent review before
done (rule 23), rule 27 security clauses (no deploy secrets to fork PRs, least-privilege
`permissions:`, SHA-pinned actions).

## Rule 16 gate verification

Not yet run for this repo. All 9 gaps from the 2026-08-22 audit closed in the same session (see
`gaps.md`), but every change was made from a Linux environment with no Windows/MSVC toolchain -
none of it has been compiled, tested, or run through `pre-commit run --all-files` for real.
Verify on Windows (local or CI) before treating this as gate-clean; do rule 16's verification
pass (deliberate lint/type/test breakage in a scratch file, confirm the hook chain rejects each,
confirm revert leaves a clean tree) at that point.
