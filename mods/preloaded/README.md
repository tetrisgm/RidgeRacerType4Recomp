# Preloaded mods

Ship reviewed packages here. They are default-disabled unless the owner chose
otherwise (`r4.enhancement.max-detail` and `r4.enhancement.widescreen` are on by
default):

```text
packages/<package-id>/<version>/
  manifest.toml
  …
```

Build wiring copies `mods/preloaded/packages` next to the game executable as
`mods/bundled/`. That tree is build output: every build wipes and re-stages it,
so nothing you place there by hand survives.

Player-installed `.psxmod` archives live in `mods/installed/`, which the
launcher owns and no build ever touches. Install them through the launcher Mods
manager rather than committing them here.

See `psxrecomp/docs/MOD_PACKAGES.md`.
