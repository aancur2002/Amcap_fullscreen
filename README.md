# Custom Lightweight Video Engine

A high-performance, ultra-lightweight DirectShow video capture viewer tailored specifically for DIY Hardware crash-cart systems. This application is an optimized, borderless-capable evolution of the classic Microsoft AMCap sample framework.

## 🚀 Features

* **Zero-Latency Video Feed:** Direct, low-level hardware hook into USB video capture cards.
* **Double-Click Fullscreen:** Double-click anywhere inside the streaming canvas to enter/exit borderless fullscreen mode instantly.
* **Menu-Driven Control:** Standard dropdown toolbar options to manually toggle fullscreen layout parameters.
* **Instant Escape:** Hit the `ESC` key to immediately exit fullscreen view and return to a standard desktop window.
* **Custom Enterprise Branding:** Complete removal of standard placeholder references, mapped entirely to custom company property metadata.
* **Perfect Coordinate Mirroring:** Removing window borders allows 1:1 absolute mouse coordinate mappings without titlebar size offsets.

---

## 📂 Directory Structure

```text
amcap-fullscreen/
├── .github/
│   └── workflows/
│       └── build.yml       # Automated GitHub Actions compilation script
├── amcap.h                 # Master direct media API declarations
├── amcap.cpp               # Core application routing logic and hooks
├── amcap.rc                # Desktop layout menu configuration script
├── resource.h              # Unique identification handles mapping
└── Makefile.mak            # Micro-compilation instructions for NMAKE
