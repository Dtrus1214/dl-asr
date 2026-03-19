## CrystalAsr

CrystalAsr is a Qt desktop app baseline for offline speech workflows.

### Current status

- Global tray app (minimize-to-tray behavior)
- Window/tray shell and settings UI
- `asr/asrengine.*` integration baseline (clipboard-driven transcription flow with placeholder backend)
- App icon (runtime) via Qt resources
- Optional Windows `.exe` icon via `icons/app.ico`
- App UI translations via Qt `.qm`

### Build requirements

- Qt (Qt 5.x or Qt 6.x)
- qmake toolchain
- sherpa-onnx C API (for offline speech pipeline work)

### Configure dependencies

Edit `CrystalAsr.pro`:

- **sherpa-onnx**: set `SHERPA_ONNX_DIR` to your install directory (must contain `include/` and `lib/`).

### Build (qmake)

From a Qt command prompt in the project directory:

```bash
qmake CrystalAsr.pro
make
```

On Windows, depending on your Qt kit/toolchain, you may build from Qt Creator or use the generated VS solution/project.

### Runtime model files

The app loads Sherpa-ONNX ASR models from the app directory under `./asr-model/`:

- `./asr-model/model.int8.onnx`
- `./asr-model/encoder.int8.onnx`
- `./asr-model/decoder.int8.onnx`
- `./asr-model/joiner.int8.onnx`
- `./asr-model/tokens.txt`
- `./asr-model/silero_vad.onnx`

### App icon

- **Runtime window/tray icon**: comes from Qt resources (`:/icons/app.svg`)
- **Windows executable icon** (Explorer): put an `.ico` at `icons/app.ico` and rebuild (the `.pro` uses `RC_ICONS` if it exists).

### Translations (App language)

App language is loaded at startup and can also be applied immediately when changed in Settings (if the `.qm` exists).

- Translation sources live in `i18n/` (see `i18n/README.md`).
- Example: Japanese translation files
  - Source: `i18n/CrystalAsr_ja.ts`
  - Binary: `i18n/CrystalAsr_ja.qm`

Generate `.qm` (Qt tools required):

```bash
lupdate CrystalAsr.pro -ts i18n/CrystalAsr_ja.ts
lrelease i18n/CrystalAsr_ja.ts -qm i18n/CrystalAsr_ja.qm
```

Place the `.qm` next to the built executable under:

- `<appdir>/i18n/CrystalAsr_ja.qm`

### Contributing

- Open an issue for bugs/features, and include reproduction steps (logs/screenshots help).
- PRs are welcome. Keep changes focused and prefer small, reviewable commits.
- UI text: wrap user-facing strings with `tr()` so they can be translated.

### License

This project is licensed under the **MIT License**. See `LICENSE`.

