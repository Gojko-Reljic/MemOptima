# MemOptima MVP Acceptance Checklist

This checklist defines the minimum evidence required before a local MemOptima MVP release is accepted. Record the result of a specific release in `docs/releases/`.

## Source and scope

- [ ] The release candidate is a committed Git revision on the local `main` branch.
- [ ] The candidate version matches the intended MVP version in `CMakeLists.txt`.
- [ ] The candidate implements only the documented MVP scope in [MVP Scope](mvp-scope.md).
- [ ] No automatic process control, system-memory modification, network telemetry, or unapproved external integration has been introduced.
- [ ] Any unrelated, user-owned untracked files are identified and excluded from the release commit and tag.

## Build configurations

- [ ] The Debug preset configures and builds successfully.
- [ ] The Release preset configures and builds successfully.
- [ ] A build with `BUILD_TESTING=OFF` configures and builds successfully.
- [ ] The Release install test installs the executable, license, README, and all user documentation into an isolated test prefix.

## Automated quality checks

- [ ] `memoptima_format_check` succeeds.
- [ ] The Debug CTest suite succeeds with AddressSanitizer and UndefinedBehaviorSanitizer enabled.
- [ ] The Release CTest suite succeeds.
- [ ] The Release `memoptima_valgrind` target succeeds for `status`, `processes`, and a controlled `monitor` interruption.
- [ ] `git diff --check` reports no whitespace errors for release changes.

## Command-line acceptance

- [ ] `memoptima --help` and `memoptima --version` return exit code `0` and write no errors.
- [ ] Invalid command-line usage returns exit code `2` and writes the explanation to standard error.
- [ ] `memoptima status` returns exit code `0` and reports RAM, swap, pressure level, and recommendation.
- [ ] `memoptima processes` returns exit code `0` and reports readable processes or the documented empty-result message.
- [ ] `memoptima monitor` displays an immediate UTC-stamped measurement and exits normally after `Ctrl+C` or `SIGINT`.
- [ ] Successful commands keep standard error empty.

## Documentation and installation

- [ ] `README.md` links to the User Guide, Metrics and Memory Pressure reference, and MVP Scope document.
- [ ] The User Guide describes build, test, installation, commands, exit codes, and troubleshooting.
- [ ] The metrics reference documents data sources, calculations, thresholds, and interpretation limits.
- [ ] The MVP Scope document describes implemented behavior, non-goals, safety and privacy boundaries, known limitations, and non-binding post-MVP directions.
- [ ] Installation with `cmake --install build/release --prefix "$HOME/.local"` is documented without requiring `sudo`.

## Local release decision

- [ ] A version-specific verification record identifies the tested Git commit, environment, tool versions, commands, and results.
- [ ] The release decision explicitly states that it is local and has not been pushed to GitHub.
- [ ] The user approves any local release tag before it is created.
