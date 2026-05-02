# Skimboute 🤙

Télécommande RF24 + contrôle différentiel 2 moteurs brushless pour skimboard motorisé.  
Manette Xbox via Bluetooth (Bluepad32) -> ESP32 TX -> RF24 -> Arduino RX -> ESCs.

---

## Architecture

```
Xbox Controller (BT)
        |
    ESP32 TX
    (Bluepad32 + RF24)
        |  ~2.4GHz RF
    Arduino RX
    (RF24 + 2x ESC)
        |
  Moteur L   Moteur R
```

---

## Matériel

| Composant | Qté | Notes |
|---|---|---|
| ESP32 | 1 | TX — Bluetooth + RF24 |
| Arduino (Uno/Nano/Mega) | 1 | RX — contrôle ESC |
| Module RF24 (nRF24L01) | 2 | SPI |
| ESC brushless | 2 | signal PWM 1000-2000µs |
| Moteur brushless | 2 | |
| Manette Xbox | 1 | Bluetooth Classic |

### Câblage RF24

**ESP32 TX:**
| RF24 | ESP32 |
|---|---|
| CE | GPIO 17 |
| CSN | GPIO 5 |
| SCK | GPIO 18 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |

**Arduino RX:**
| RF24 | Arduino |
|---|---|
| CE | D9 |
| CSN | D10 |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |

**ESC:**
| ESC | Arduino |
|---|---|
| Moteur Gauche | D6 |
| Moteur Droit | D7 |

---

## Dépendances

### ESP32 (TX)
- [Bluepad32](https://github.com/ricardoquesada/bluepad32-arduino) — réception manette Xbox BT
- [RF24](https://github.com/nRF24/RF24) — transmission radio

Board manager URL Bluepad32:
```
https://raw.githubusercontent.com/ricardoquesada/bluepad32-arduino/main/boards_manager/package_esp32_bluepad32_index.json
```

### Arduino (RX)
- [RF24](https://github.com/nRF24/RF24) — réception radio
- [Servo](https://www.arduino.cc/reference/en/libraries/servo/) — contrôle ESC PWM

---

## Logique de contrôle

### Mapping axes manette
| Input | Action |
|---|---|
| Stick G haut | Avance (L+ R+) |
| Stick G bas | Recule (L- R-) |
| Stick G droite | Tourne droite (L+ R-) |
| Stick G gauche | Tourne gauche (L- R+) |
| Centre (deadzone) | Stop |

### Mixage différentiel
```
speedL = speedY + speedX
speedR = speedY - speedX
```

### PWM ESC
```
stop    -> 1000µs
neutre  -> 1500µs
max     -> 2000µs
```

### Failsafe
Coupure RF > 500ms -> ESC 1000µs (stop immédiat).

---

## Configuration

### TX (`tx/tx.ino`)
```cpp
#define DEBUG true        // logs Serial
const int DEADZONE = 100; // sensibilité stick
```

### RX (`rx/rx.ino`)
```cpp
#define DEBUG true         // logs Serial
const int DEADZONE = 150;  // ajuster selon jitter
const int FAILSAFE_MS = 500;
const int CENTER_X = 1830; // repos joystick X (si analogique)
const int CENTER_Y = 1850; // repos joystick Y
```

---

## Structure

```
skimboute/
├── tx/
│   └── tx.ino        # ESP32 — Bluepad32 + RF24
├── rx/
│   └── rx.ino        # Arduino — RF24 + ESC
└── README.md
```

---

## TODO

- [ ] Brancher 2ème moteur + décommenter ESC dans RX
- [ ] Tester failsafe RF en conditions réelles
- [ ] Ajuster DEADZONE selon retour terrain
- [ ] Ajouter contrôle vitesse max (trim)
- [ ] Boîtier étanche

---

## Licence

WTFPL — fais ce que tu veux.
