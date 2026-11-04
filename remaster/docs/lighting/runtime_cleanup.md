# Runtime cleanup (November 2026)

This cleanup removes superseded implementation paths, not supported capture/profile
formats, authored geometry, or current diagnostic/reference rendering modes.

## Boundaries

- Sample accumulation was removed from production dispatch in `e9b351cb`.
  Its shader, one-iteration sample loop, unused ping-pong texture, dead UI actions
  and always-zero timing metric should not remain a second lighting implementation.
- Historical `sample_accumulation` profile values are validated and retained as
  read-only `legacySampleAccumulation` evidence for captures/benchmark identities,
  but omitted when saving profiles. Scene settings, Undo and rendering no longer
  treat them as controls. Captures preserve either strict boolean wire value
  byte-for-byte, without changing the binary layout or rejecting old captures.
- Static dungeon declaration/effect analysis belongs to the read-only ALttP
  inventory tool. Declarations alone cannot authorize geometry; live rendering
  must not compute and retain an assessment with no consumer.
- Historical emission-to-sphere approximations belong to test reference headers,
  not production frame or offscreen-source code. Current tests must exercise
  actual channel selection and exact hidden-source transport independently.
- `ALTTP_TEST_PRIOR_*` switches resurrect historical behavior. Git history and
  historical reports retain those implementations; maintained work counters,
  forced-cold paths and independent differential oracles are a different category.
  The 17 historical switches were retired with current behavior unconditional;
  old report commands requiring them must use their recorded historical commit.

## Validation policy

Establish the current full portable/Metal result, supplemental test results and
Python tooling results before editing. Preserve every previously passing
behavioral check; record pre-existing failures without weakening bounds or
silently excluding them. Use the immutable stress-v2 manifest and baseline for
native Release performance comparisons; do not rewrite profiles or rebaseline
to accept a regression. Keep compact logs, hashes and comparisons under ignored
`build/remaster-cleanup/`, and remove superseded disposable binaries/apps after
replacement verification.

The curated gate is not complete-room height approval or a performance-budget
certification. See [performance stress suite](performance_stress_suite.md).
Baseline failure accounting and the validation scope are documented in
[cleanup validation](runtime_cleanup_validation.md).
