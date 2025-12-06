# Software Distribution Plan

## Purpose
This document outlines how the **Wireless Integration Subsystem** firmware for the *Walker Fall Detection System* will be distributed, versioned, and maintained throughout the semester.

---

##  Repository
**GitHub:** [ananth3177/walker-fall-detection](https://github.com/ananth3177/walker-fall-detection)

All source code, configuration files, and documentation will be managed in this repository to ensure consistency and collaboration between subsystem teams.

**Structure:**
src/ → ESP32 firmware source
docs/ → Design documents & diagrams
test/ → Test logs & demo data
platformio.ini → Build configuration

---

##  Development & Version Control
- Environment: **PlatformIO** on **VS Code**  
- Language: **C++ (Arduino framework)**  
- Hardware: **ESP32 NodeMCU (ESP32-32S)**  
- Networking: **Wi-Fi (IEEE 802.11 b/g/n)** → **HTTPS POST → IFTTT → SMS**  

Git branching model:
- `main` → stable, demo-ready code  
- `dev` → current development version  
- `feature/...` → temporary branches for subsystem updates  

Each milestone (Midterm Demo, Final Integration, Final Testing) will be tagged (`v1.0`, `v2.0`, etc.) for traceability.

---

##  Milestones
| Week | Task | Deliverable |
|------|------|-------------|
| 6–7 | ESP32 Wi-Fi & IFTTT setup | Button-triggered SMS test |
| 8 | Midterm demo | Working SMS alert |
| 9–11 | Lockout + buzzer integration | Stable v1.1 |
| 12–16 | Final testing & documentation | Release v2.0 |

---

##  Deployment
Firmware is flashed to the ESP32 via PlatformIO.  
Upon startup, the device connects to Wi-Fi, monitors tilt/button input, and sends an HTTPS POST to IFTTT to trigger SMS alerts.  
If Wi-Fi is unavailable, the buzzer/LED provide local alerts until reconnection.

---

##  Maintenance
All final code, diagrams, and documentation will remain publicly accessible in the GitHub repository for future use, evaluation or project expansion
