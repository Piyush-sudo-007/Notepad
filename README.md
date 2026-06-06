# 📝 SmartPad

[![GitHub release](https://img.shields.io/github/v/release/piyush-sudo-007/smartpad?include_prereleases&style=flat-square)](https://github.com/Piyush-sudo-007/Smartpad/releases/tag/v1.1.0)
[![GitHub issues](https://img.shields.io/github/issues/piyush-sudo-007/smartpad?style=flat-square)](https://github.com/piyush-sudo-007/smartpad/issues)

SmartPad is a lightweight, high-performance desktop text editor supercharged with completely offline, private AI next-word predictions. Built entirely in C++ using the Qt framework, it leverages an embedded local AI runtime environment to adapt directly to your typing habits without leaking data to cloud servers.

---

## 🚀 Download & Installation

Get the latest version compiled for global distribution on Windows.

1. Navigate to the [Latest Releases Page](https://github.com/Piyush-sudo-007/Smartpad/releases/tag/v1.1.0).
2. Download the compressed binary package: `SmartPad_Setup.zip`.
3. Extract the contents and double-click the setup installer to run the configuration wizard.

### ⚠️ Bypassing Windows SmartScreen Warnings

Because this application is built independently and has not established an official commercial developer reputation registry with Microsoft yet, Windows Defender Defender SmartScreen will show a blue **"Windows protected your PC"** pop-up when starting the installer.

- **To Install anyway:** Click **"More info"** inside the warning message box, then click the newly revealed **"Run anyway"** button. The application executes entirely locally and safely.

---

## ✨ Features

- **Offline Next-Word Prediction:** Powered natively via an embedded local causal AI framework running directly inside your CPU.
- **Intelligent Context Switching:** Autodetects your current writing profile. The editor shifts runtime profiles dynamically between general composition (`Blog` mode) and syntax structures (`Code` mode) when it flags programming statements (`#include`, `import `, `using `).
- **Real-Time Local Personalization:** Includes an embedded database connection tracking user input metrics to bias token probabilities on subsequent predictions, adapting instantly to your personal vocabulary.
- **Full Text Utility Suite:** Includes real-time character counters, line-column position tracking status bars, text/background custom styling brushes, page setup layout rendering, and hardware printer system configurations.

---

## 🛠️ Built With & Deep Architecture

SmartPad is built using native platform libraries to guarantee efficiency and high-speed execution profiles:

- **[Qt 6 (C++)](https://www.qt.io/)** - High-fidelity GUI widgets framework, layout engines, and asynchronous architecture.
- **[ONNX Runtime Engine](https://onnxruntime.ai/)** - Direct local execution library optimized for loading causal language prediction parameters seamlessly over host processing architectures.
- **[SQLite3 Embedded DB](https://www.sqlite.org/)** - Lightweight data management engine handling localized vocabulary logs to fine-tune active token probability generation layers dynamically.
- **[Hugging Face / Python Optimization Stack](https://huggingface.co/)** - GPT-2 language modeling dependencies fine-tuned using dual mixed streams (Alpaca GPT-4 instructions & structural code text feedback matrices).

---

## 💻 Technical Build & Local Compilations

If you want to build and compile the software from source code fields manually:

### Prerequisites

- Qt Creator IDE paired with a **MinGW 64-bit** compilation toolchain.
- Tokenizer headers and pre-built `onnxruntime.dll` references mapped within the relative project layout parameters.

### Build via Qt Creator

1. Launch Qt Creator IDE.
2. Select **Open Project** and navigate to your source directory to pull in `NotePad.pro`.
3. Choose your default local kit, initialize structural configurations, then press **Build and Run**.

### Build via Command-Line Environment (MinGW)

```bash
qmake
mingw32-make
./NotePad.exe
```
