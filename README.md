# Traffic Arcade Simulator

Un simulator auto tip "endless runner" (arcade) realizat în C++ pe baza bibliotecii `ncurses` pentru redarea graficii block-pixel în terminal.

## 🚀 Caracteristici principale

- **Mecanici de Condus Realiste:**
  - Control complet asupra benzilor de circulație (sistem de 4 benzi cu ambele sensuri).
  - Model matematic de accelerare (inclusiv calcule liniare de RPM, turații ale motorului în funcție de treapta de viteză și inerție aerodinamică).
  - Tranziție dinamică între condus vertical și orizontal prin intersecții.

- **Reguli de Trafic și Penalizări:**
  - Semne de circulație respectate de inteligența artificială și de jucător: semafoare montate sus, semafoare de colț, **STOP** (roșu octogonal) și **Cedează Trecerea** (triunghi galben întors).
  - **Sistem de Karma:** Primești puncte bonus dacă acorzi prioritate și încetinești la semafoarele roșii.
  - **Poliția în trafic:** Dacă treci cu o viteză mai mare de 90 km/h pe lângă o mașină de poliție (vopsită în alb și cu girofar albastru-roșu care pulsează), vei fi penalizat sever pe moment!
  - 5 penalizări duc direct la **Game Over**. De asemenea, orice coliziune frontală îți încheie runda.

- **Detalii Grafice (Terminal):**
  - Grafică detaliată, 100% în consolă cu sprijin ANSI (256-culori) prin `ncurses`.
  - Bord interactiv (`DASHBOARD`) cu indicatori vizuali pentru viteză (lag de ac realist), treaptă curentă de viteză, scor, penalizări și Karma.
  - Elementele de decor se generează procedural: copaci, iarbă, case, blocuri, parcări și lacuri de apă.

## 🛠️ Cerințe de sistem

- Un terminal Linux sau un subsistem WSL (Windows Subsystem for Linux) pe Windows.
- Compilator pentru C++ (recomandat `g++`).
- **CMake** instalat (minim versiunea 3.10).
- Biblioteca de terminal **ncurses** (de preferat pachetul `libncurses5-dev` sau `libncursesw5-dev` pe sisteme Ubuntu/Debian).

## 📥 Instalare și Compilare

1. **Clonează repository-ul** și intră în dosarul proiectului:
   ```bash
   git clone <URL_GIT>
   cd proiect_POO
   ```

2. **Generează fișierele de configurare pentru compilator folosind CMake:**
   ```bash
   cmake -S . -B build
   ```

3. **Compilează codul sursă:**
   ```bash
   cmake --build build
   ```

## 🎮 Cum se joacă?

Rulează executabilul generat (din mediul WSL sau Linux):
```bash
./build/TrafficSimulator
```

### Controale:
- **[W]** / **[Săgeată SUS]** - Accelerează.
- **[S]** / **[Săgeată JOS]** - Frânează.
- **[A]** / **[Săgeată STÂNGA]** - Schimbă banda spre stânga.
- **[D]** / **[Săgeată DREAPTA]** - Schimbă banda spre dreapta.
- **[P]** - Pune jocul pe pauză.
- **[Q]** - Ieși forțat din joc.
- **[A / D] sau [W / S]** în intersecții (pentru a vira stânga/dreapta în funcție de sensul de direcție de pe busolă).

### Obiectiv:
Supraviețuiește cât mai mult în trafic, nu te ciocni de inamici (tiruri, mașini de trafic orizontal, conuri) și fii respectuos cu regulile de trafic! Scorurile tale maxime vor fi reținute sesiunea curentă.

---
_Realizat pentru proiectul de Programare Orientată pe Obiecte (POO)._
