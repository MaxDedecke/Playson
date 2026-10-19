# 🔊 Playson — Sonos Master Bridge for FL Studio

**Playson** ist ein extrem schlankes, natives Audio-Plugin (**CLAP-Format**) für FL Studio und moderne DAWs, das deinen aktuellen Timeline-Loop mit **einem Klick direkt vom Master-Kanal auf deinen Sonos WLAN-Speaker** schickt.

Entwickelt **ohne schweres JUCE-Framework**, mit 0 % Bloat, blitzschnellem C++17 und nativer Windows-Netzwerkintegration (`WinHTTP`).

---

## ✨ Highlights

* **1-Klick-Workflow:** Klicke im Mixer auf **„▶ BOUNCE TO SONOS“** — das Plugin fängt deinen Loop auf, rendert den WAV-Snapshot im RAM und sendet ihn sofort an Sonos.
* **Kein JUCE, kein Bloat:** 100 % Open-Source nach dem modernen **CLAP-Standard** (mitentwickelt von Image-Line / FL Studio).
* **Verlustfreie HiFi-Qualität:** Unkomprimiertes 16-Bit / 44,1 kHz PCM-WAV (Sonos spielt WAV nativ ohne Konvertierung).
* **Automatischer Transport-Sync:** Erkennt Tempo (BPM), Songposition und Loop-Ende automatisch aus FL Studio.
* **Native Windows-Integration:** Verwendet `WinHTTP` und Direct-GDI+ — keine externen DLLs oder Abhängigkeiten nötig.

---

## 🚀 Installation in FL Studio

### Option A: Automatischer Cloud-Build (Empfohlen)
Jeder Push oder Release auf GitHub baut die fertige `Playson.clap`-Datei automatisch via **GitHub Actions**:
1. Gehe auf GitHub im Reiter **[Actions](../../actions)** auf den neuesten Build (oder lade das Release herunter).
2. Lade das Artefakt `Playson-Windows-x64.zip` herunter und entpacke `Playson.clap`.
3. Kopiere `Playson.clap` in den Windows CLAP-Ordner:
   ```text
   C:\Program Files\Common Files\CLAP\
   ```
   *(oder doppelklicke einfach auf `install_to_fl_studio.bat`)*.

### Option B: Lokal bauen
Falls CMake und ein C++-Compiler (MSVC/Clang) installiert sind:
```cmd
build.bat
```

---

## 🎹 Verwendung in FL Studio

1. **Connector starten:** Stelle sicher, dass der **Sonos Musik Connector** im Hintergrund läuft (`http://localhost:5005`).
2. **Plugin einfügen:**
   * Öffne den FL Studio Mixer.
   * Wähle auf dem **Master-Kanal** einen freien Effektslot.
   * Klicke auf `More plugins...` -> `Manage plugins` -> `Find installed plugins` (beim ersten Mal).
   * Lade **Playson** auf den Master.
3. **Sound-Check auf Sonos:**
   * Markiere deinen gewünschten Takt-Bereich (z. B. Bar 1 bis 16) in der FL Studio Playlist.
   * Klicke im Plugin-Fenster auf den großen orangen Button **„▶ BOUNCE TO SONOS“**.
   * Sobald der Loop durchgelaufen ist (oder wenn du Stop drückst), feuert Playson den Snapshot sofort auf deinen Sonos Play:1 Lautsprecher!

---

## 🛠️ Technische Architektur

```text
[ FL Studio Master-Bus ]
         │ (Stereo Float Audio + DAW Transport)
         ▼
[ Playson.clap Plugin ]
         │  1. Audio Buffer Capture (Loop Bar 1 -> End)
         │  2. High-speed in-memory WAV-Encoding
         │  3. WinHTTP POST -> localhost:5005/api/upload
         ▼
[ Sonos Musik Connector ]
         │ (UPnP Streaming / AVTransport)
         ▼
[ Sonos WLAN Speaker (Play:1) ]
```

---

## 📄 Lizenz
MIT License — Copyright (c) 2026 Max Dedecke
