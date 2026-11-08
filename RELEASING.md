# Release process

1. Update the version in `desktop/pkgbuild/PKGBUILD`, `desktop/src/main.cpp`,
   README installation examples and CHANGELOG. Set `pkgrel=1` for a new version.
2. Run the development checks listed in README and `pio run` locally. Do not
   distribute the resulting firmware: it contains local Wi-Fi credentials.
3. Build the desktop package with `cd desktop/pkgbuild && makepkg -f`.
4. Inspect `pacman -Qip` and `bsdtar -tf` output. Verify the package includes the
   sync script, setup helper, user service, template and bar plugin. It must
   contain no personal config, secrets, build directories or private firmware.
5. Test first-run setup in a temporary home and migration with a copy of legacy
   config. Install locally and verify the packaged service uses `/usr/lib`.
6. Commit and push the release changes. Wait for the clean Arch CI job to pass.
7. Tag the tested commit `v<version>`, then prepare a draft GitHub release with
   the source archive, x86_64 package, release notes and `SHA256SUMS`. If using
   the CI-built package, test the exact uploaded artifact too.
8. Review assets and checksums, then publish the release. A package built on
   Arch's rolling libraries requires an up-to-date Arch/Omarchy system.

The release source archive must contain tracked source only. Use
`git archive --format=tar.gz --prefix=arch-sign-<version>/ <tag>`.
The package does not modify user config in pacman install hooks. Device setup
runs as the signed-in user, through the GUI or `arch-sign-setup`.

Firmware is source-only in this release because each installation needs its
own credentials, data pin, LED count and startup logo ranges. A future generic
firmware release would require provisioning those settings at runtime first.
