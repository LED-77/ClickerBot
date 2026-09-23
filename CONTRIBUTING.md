# Contributing

Thanks for taking a look at the project. It is a hobby device built by one
person, so please keep expectations modest: issues and pull requests are read,
but there is no support schedule.

## Reporting a problem

Please use the issue template and include:

- what you expected and what actually happened,
- the **firmware version** (the boot banner prints it, or look for the
  `CLICKERFW:` string inside the `.bin`),
- which board/module you use, and the flash chip ID if you can read it with
  `esptool.py flash_id`,
- whether the problem happens after a fresh flash with `erase_flash`.

The flash-chip quirk described in [docs/BUILD.md](docs/BUILD.md#the-40-mhz--dio-rule)
explains a surprising number of "it works but saves nothing" reports — worth
checking first.

## Pull requests

- Build locally with `pio run` and make sure the CI check is green.
- Keep the existing style: comments are **bilingual (Russian first, then
  English)**, explained below.
- One logical change per pull request; describe the *why*, not only the *what*.
- New skins should go through the `Skin` interface and be registered in
  `SkinRegistry.cpp` — nothing else needs to know about them.

## Comment style

Every comment carries both languages, because the project is published
internationally while the author thinks in Russian:

```cpp
// Ниже этого заряда — перечёркнутая батарея на заставке и сразу deep sleep.
// Below this level: crossed-out battery on the splash and straight to deep sleep.
```

Short trailing comments use a single line: `// всплытие от точки к уголку / rise from the point to the corner`.
Section headers combine both: `// --- Сон / Sleep ---`.

When you change a comment, update both halves — a stale translation is worse than
none.

## License

By contributing you agree that your changes are released under the MIT license
of this repository (see [LICENSE](LICENSE)).
