# Remaster code layout

- **`runtime/`**: production remaster implementation and shared data formats,
  including rendering support, lighting, height adjustment, profile editing,
  capture/replay support, and debug views. The portable implementation is mostly
  header-based; platform presentation code remains in its frontend directory.
- **`tests/`**: standalone C++ regression tests, captured-scene probes, corpus
  audits, test-only helper headers, and their Python gate/differential scripts.
- **`profiles/`**: authored game profiles (formerly the root `Profiles/` folder).
- **`tools/`**: command-line utilities, authoring workbenches, benchmark runners,
  and game-specific tooling (formerly the root `tools/` folder).
- **`docs/authoring/`**: [authoring and inspection](docs/authoring/00-index.md).
- **`docs/geometry/`**: [geometry and placement](docs/geometry/00-index.md).
- **`docs/lighting/`**: [lighting and performance](docs/lighting/00-index.md).
- **Top-level Markdown files**: additional game-specific review/tooling notes.
- **`historic-investigations/`**: room/capture investigations and older repair,
  audit, editor and performance reports, with a [table of contents](historic-investigations/00-index.md).
  These are historical evidence, not a current issue list. Captured evidence and
  generated outputs have not moved; commands retain repository-root paths.

Run the curated portable suite from the repository root:

```sh
python3 remaster/tools/run_remaster_tests.py --suite full
```

Compile an individual portable test directly when needed:

```sh
c++ -std=c++17 remaster/tests/layer_editor_test.cpp -o build/layer-editor-test
build/layer-editor-test
```

Some probes need explicitly pinned captures and sidecars. Moving their source
does not make them fixture-free or establish complete-room height approval.
Platform-specific tests remain in `macosx/`; game-tool tests remain in
`remaster/tools/ALttP/`. See [the test guide](tools/remaster-tests.md) for suite
selection and limitations.

Door frame authoring uses the **Doorway** role; the separate **Door** role is
reserved for actual door leaves/barriers. See [doorway authoring](docs/authoring/doorway_authoring.md)
for directional support contacts in the Metadata inspector and migration behavior.
