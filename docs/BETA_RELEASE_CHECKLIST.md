# Beta release checklist

This fork is experimental. Do not publish a release until the checks below pass
on the exact commit to be released.

- Review the fork's changes against FreesmTeam/FreesmLauncher and confirm the
  intended UI and dependency changes are retained.
- Run Build, CodeQL and Clang-Tidy on the release candidate. Investigate all
  failures, including flaky tests, instead of relying on a partial matrix.
- Exercise logs, search, line coloring, scrolling and log buffer truncation;
  also check the modified instance and resource views.
- Check the produced Linux, Windows and macOS packages on their target systems,
  including application startup and a game launch. Verify signing separately.
- Verify that CI does not publish the vcpkg cache to GitHub Packages. Cache
  reuse is currently disabled for the Windows MSVC and macOS vcpkg builds.
- Choose a version tag containing `-beta` (for example, `v1.0.0-beta.1`).
  The tag-triggered release workflow creates a **draft prerelease**; inspect
  its assets, checksums, version and notes before publishing it manually.
- Do not publish to upstream FreesmTeam distribution channels from this fork.

No tag or release is created by this checklist.
