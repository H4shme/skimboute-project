# Skimboute 🏄

Skimboard motorisé télécommandé via RF24 + dashboard web.

---

## Architecture
```
Joystick (ESP32 TX)──RF24──> Arduino RX ──> 2x ESC ──> 2x Moteurs
         |
      WiFi AP
         |
    Browser (dashboard)
```

---

## Matériel
| Composant | Qté |
|---|---|
| ESP32 (TX + WebServer) | 1 |
| Arduino Uno/Nano (RX) | 1 |
| Module nRF24L01 | 2 |
| ESC brushless | 2 |
| Moteur brushless | 2 |
| Capteur humidité sol | 1 |
| Joystick analogique XY+SW | 1 |

---

## Câblage

**RF24 — ESP32 TX:**
| RF24 | ESP32 |
|---|---|
| CE | GPIO 17 |
| CSN | GPIO 5 |
| SCK | GPIO 18 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |

**RF24 — Arduino RX:**
| RF24 | Arduino |
|---|---|
| CE | D9 |
| CSN | D10 |
| SCK | D13 |
| MOSI | D11 |
| MISO | D12 |

**Joystick — ESP32:**
| Joystick | ESP32 |
|---|---|
| VRX | GPIO 34 |
| VRY | GPIO 35 |
| SW | GPIO 32 |
| VCC | 3.3V |
| GND | GND |

**Autres:**
| Composant | Pin |
|---|---|
| ESC Gauche | Arduino D6 |
| ESC Droit | Arduino D7 |
| Capteur humidité SIG | Arduino A0 |

---

## Dépendances
- [RF24](https://github.com/nRF24/RF24)
- Servo (Arduino lib manager)

---

## Contrôle
| Joystick | Action |
|---|---|
| Haut | Avance |
| Bas | Recule |
| Droite | Tourne droite |
| Gauche | Tourne gauche |
| Bouton SW (appui) | Toggle arrêt d'urgence |

**Mixage différentiel:**
```
motorL = speedY + speedX
motorR = speedY - speedX
```

**PWM ESC:** 1000µs=stop · 1500µs=neutre · 2000µs=plein gaz

**Failsafe:** coupure RF >500ms → ESC 1000µs

---

## Dashboard Web
Connecte ton tel au WiFi `Skimboute` / `skimboute1423` → ouvre `192.168.4.1`

Affiche: direction, humidité %, µs moteurs, état radio, logs défilants, ping ESP↔Arduino toutes les 10s.

---

## Debug flags (TX + RX)
```cpp
#define DEBUG_TX    true  // valeurs joystick
#define DEBUG_RF    true  // ACK radio
#define DEBUG_HUM   true  // humidité
#define DEBUG_NONE  false // tout couper (prod)
```

---

## TODO
- [ ] Décommenter ESC dans RX quand moteurs branchés
- [ ] Ajuster `CENTER_X/Y` selon joystick réel
- [ ] Ajuster `DEADZONE` selon jitter terrain
- [ ] Ajouter GPS Neo-6M
- [ ] Boîtier étanche
