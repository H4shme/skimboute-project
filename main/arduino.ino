/*
ARDUINO CODE

 ________________
| RF24 | Arduino |
|------|---------|
| CE   | D9      |
| CSN  | D10     |
| SCK  | D13     |
| MOSI | D11     |
| MISO | D12     |
/////////////////
 _________________________
| Composant  |    Pin     |
|------------|------------|
| ESC Gauche | Arduino D6 |
| ESC Droit  | Arduino D7 |
//////////////////////////
 _________________________
| Capteur    |            |
| humidité   | Arduino A0 |
| SIG        |            |
//////////////////////////

*/

#include <SPI.h>
#include <RF24.h>
#include <Servo.h>

// ----------------------------------------------------------------
//  FLAGS DE DEBUG
//  Mettre à true/false pour activer/désactiver chaque catégorie
//  DEBUG_NONE à true = silence total (override tous les autres)
// ----------------------------------------------------------------
#define DEBUG_RX      true    // Affiche les valeurs brutes reçues du joystick
#define DEBUG_CALC    true    // Affiche les vitesses calculées (speedX, speedY)
#define DEBUG_MOTORS  true    // Affiche les µs envoyés aux ESC
#define DEBUG_DIR     true    // Affiche la direction interprétée (AVANCE, DROITE...)
#define DEBUG_HUM     true    // Affiche la valeur brute du capteur humidité
#define DEBUG_NONE    false   // true = désactive tout le Serial.print

// ----------------------------------------------------------------
//  RADIO RF24
//  CE = pin 9, CSN = pin 10 
// ----------------------------------------------------------------
RF24 radio(9, 10);
const byte address[6] = "00001"; // Adresse commune émetteur/récepteur

// ----------------------------------------------------------------
//  ESC (Electronic Speed Controllers)
//  Les ESC reçoivent des signaux PWM entre 1000µs (arrêt) et 2000µs (pleine vitesse)
//  1500µs = vitesse nulle (point mort)
// ----------------------------------------------------------------
Servo escL, escR;                 // Objets Servo pour piloter les ESC
const int ESC_L_PIN    = 6;       // Pin PWM ESC gauche
const int ESC_R_PIN    = 7;       // Pin PWM ESC droit

// ----------------------------------------------------------------
//  ACTIVATION MOTEURS
//  Permet de tester le code sans brancher les ESC physiquement
//  false = simulation (Serial uniquement), true = signal PWM réel envoyé
// ----------------------------------------------------------------
bool enableMotorL = true;         // Activer le moteur gauche
bool enableMotorR = true;         // Activer le moteur droit

// ----------------------------------------------------------------
//  CONSTANTES DE COMPORTEMENT
// ----------------------------------------------------------------
const int FAILSAFE_MS  = 500;     // Délai sans signal avant arrêt d'urgence (ms)
const int CENTER_X     = 0;       // TX envoie valeurs déjà centrées (cx = rawX - ADC_CENTER)
const int CENTER_Y     = 0;       // TX envoie valeurs déjà centrées (cy = rawY - ADC_CENTER)
const int DEADZONE     = 400;     // Zone morte autour du centre (ignore les micro-déviations)
const int HUMIDITY_PIN = A0;      // Pin analogique du capteur humidité

// ----------------------------------------------------------------
//  STRUCTURES DE DONNÉES RF24
//  DataIn  : paquet reçu depuis l'émetteur (joystick + urgence)
//  DataAck : paquet retour (acquittement) envoyé à l'émetteur
//  __attribute__((packed)) = pas de padding mémoire → taille exacte sur le bus SPI
// ----------------------------------------------------------------
struct DataIn {
  int16_t x;          // Valeur brute axe X du joystick (0-4095, 12 bits)
  int16_t y;          // Valeur brute axe Y du joystick (0-4095, 12 bits)
  bool    emergency;  // true = arrêt d'urgence demandé
} __attribute__((packed));

struct DataAck {
  int16_t humidity;   // Valeur brute du capteur humidité (0-1023, 10 bits)
} __attribute__((packed));

DataIn  data;            // Buffer de réception
DataAck ackData;         // Buffer d'acquittement

// ----------------------------------------------------------------
//  ÉTAT INTERNE
// ----------------------------------------------------------------
unsigned long lastRX   = 0;     // Timestamp du dernier paquet reçu (ms)
bool failsafeActive    = false; // true si le failsafe est déjà déclenché (évite le spam)

// ----------------------------------------------------------------
//  FONCTION : joystickToSpeed
//  Convertit une valeur déjà centrée en vitesse [-100, +100]
//  Applique la zone morte autour du centre
//  raw    : valeur centrée reçue (−ADC_CENTER .. +ADC_CENTER)
//  center : toujours 0 (TX envoie cx/cy déjà centrés)
// ----------------------------------------------------------------
int joystickToSpeed(int raw, int center) {
  int offset = raw - center; // Écart par rapport au centre

  // Dans la zone morte → vitesse nulle
  if (abs(offset) < DEADZONE) return 0;

  // Côté négatif : mappe [−2047 .. −DEADZONE] → [−100 .. 0]
  if (offset < 0) return map(offset, -2047, -DEADZONE, -100, 0);

  // Côté positif : mappe [+DEADZONE .. +2047] → [0 .. +100]
  else            return map(offset, DEADZONE, 2047, 0, 100);
}

// ----------------------------------------------------------------
//  FONCTION : speedToMicros
//  Convertit une vitesse [-100, +100] en signal PWM [1000, 2000] µs
//  constrain() assure qu'on reste dans la plage valide ESC
// ----------------------------------------------------------------
int speedToMicros(int speed) {
  return constrain(map(speed, -100, 100, 1000, 2000), 1000, 2000);
}

// ----------------------------------------------------------------
//  FONCTION : emergencyStop
//  Coupe les moteurs immédiatement
//  Appelée sur data.emergency == true ou perte de signal (failsafe)
// ----------------------------------------------------------------
void emergencyStop() {
  // Envoie 1000µs aux ESC = position d'arrêt total
  if (enableMotorL) escL.writeMicroseconds(1000);
  if (enableMotorR) escR.writeMicroseconds(1000);

  if (!DEBUG_NONE) Serial.println("[URGENCE] STOP moteurs");
}

// ----------------------------------------------------------------
//  SETUP
//  Initialisation ESC + Radio
// ----------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // --- Initialisation ESC ---
  // attach() démarre la génération du signal PWM sur la pin
  escL.attach(ESC_L_PIN);
  escR.attach(ESC_R_PIN);

  // Séquence d'armement ESC :
  // 1) Envoyer 1000µs (signal minimal) pour signaler "prêt à armer"
  if (enableMotorL) escL.writeMicroseconds(1000);
  if (enableMotorR) escR.writeMicroseconds(1000);
  delay(2000); // Attente pour que l'ESC reconnaisse le signal minimal

  // 2) Passer à 1500µs (point mort) = ESC armé, moteur à l'arrêt
  if (enableMotorL) escL.writeMicroseconds(1500);
  if (enableMotorR) escR.writeMicroseconds(1500);
  delay(3000); // Délai de stabilisation post-armement

  if (!DEBUG_NONE) {
    if (enableMotorL || enableMotorR)
      Serial.println("[OK] ESC armé (réel)");
    else
      Serial.println("[OK] ESC en simulation (enableMotorX = false)");
  }

  // --- Initialisation Radio ---
  if (!radio.begin()) {
    Serial.println("[ERR] radio FAIL"); // Vérifier câblage SPI
    while(1) { delay(100); }           // Blocage : rien à faire sans radio
  }

  radio.setPALevel(RF24_PA_MIN);              // Puissance minimale (test en proximité)
  radio.enableAckPayload();                   // Active les acquittements avec données (ackData)
  radio.openReadingPipe(1, address);          // Écoute sur l'adresse définie
  radio.startListening();                     // Mode récepteur

  if (!DEBUG_NONE) Serial.println("[OK] listening");
}

// ----------------------------------------------------------------
//  LOOP
//  Cycle principal : prépare ACK → lit radio → calcule → commande
// ----------------------------------------------------------------
void loop() {

  // --- Prépare le paquet d'acquittement ---
  // Lit le capteur humidité AVANT de vérifier radio.available()
  // Ainsi l'ACK est toujours fresh quand un paquet arrive
  ackData.humidity = (int16_t)analogRead(HUMIDITY_PIN);
  radio.writeAckPayload(1, &ackData, sizeof(ackData)); // Charge l'ACK dans le buffer RF24

  // --- Réception d'un paquet ---
  if (radio.available()) {
    radio.read(&data, sizeof(data)); // Lit le paquet dans la struct DataIn
    lastRX         = millis();       // Met à jour le watchdog failsafe
    failsafeActive = false;          // Signal reçu → failsafe inactif

    // --- Arrêt d'urgence prioritaire ---
    // Si le bouton d'urgence est pressé côté émetteur, on coupe tout
    if (data.emergency) {
      emergencyStop();
      return; // Ignore le reste du calcul moteur
    }

    // --- Calcul des vitesses ---
    // Inversion du signe selon le sens de montage du joystick (à ajuster si inversé)
    int speedY = -joystickToSpeed(data.x, CENTER_X); // Axe avant/arrière (data.x pilote Y rover)
    int speedX = -joystickToSpeed(data.y, CENTER_Y); // Axe rotation (data.y pilote X rover)

    // Mélange différentiel (tank drive) :
    //   Moteur gauche = avance + rotation droite
    //   Moteur droit  = avance - rotation droite
    int speedL = constrain(speedY + speedX, -100, 100);
    int speedR = constrain(speedY - speedX, -100, 100);

    // Conversion vitesse → µs PWM
    int microsL = speedToMicros(speedL);
    int microsR = speedToMicros(speedR);

    // --- Debug ---
    if (!DEBUG_NONE && DEBUG_RX) {
      Serial.print("[RX] rawX="); Serial.print(data.x);
      Serial.print(" rawY="); Serial.println(data.y);
    }
    if (!DEBUG_NONE && DEBUG_HUM) {
      Serial.print("[HUM RAW] "); Serial.println(ackData.humidity);
    }
    if (!DEBUG_NONE && DEBUG_CALC) {
      Serial.print("[CALC] speedY="); Serial.print(speedY);
      Serial.print(" speedX="); Serial.println(speedX);
    }
    if (!DEBUG_NONE && DEBUG_MOTORS) {
      Serial.print("[MOTORS] L="); Serial.print(speedL);
      Serial.print("% (");        Serial.print(microsL);
      Serial.print("µs)  R=");    Serial.print(speedR);
      Serial.print("% (");        Serial.print(microsR);
      Serial.println("µs)");
    }
    if (!DEBUG_NONE && DEBUG_DIR) {
      if      (speedY == 0 && speedX == 0)      Serial.println("[DIR] IMMOBILE");
      else if (speedY > 0  && abs(speedX) < 20) Serial.println("[DIR] AVANCE");
      else if (speedY < 0  && abs(speedX) < 20) Serial.println("[DIR] RECULE");
      else if (speedX > 0  && abs(speedY) < 20) Serial.println("[DIR] DROITE");
      else if (speedX < 0  && abs(speedY) < 20) Serial.println("[DIR] GAUCHE");
      else                                       Serial.println("[DIR] MIXTE");
    }

    // --- Commande ESC ---
    // Envoie le signal PWM uniquement si le moteur est activé
    if (enableMotorL) escL.writeMicroseconds(microsL);
    if (enableMotorR) escR.writeMicroseconds(microsR);

  } else {
    // --- Failsafe : pas de signal radio ---
    // Si aucun paquet reçu depuis FAILSAFE_MS ms → coupe les moteurs
    if (millis() - lastRX > FAILSAFE_MS) {
      if (!failsafeActive) {          // Déclenche une seule fois (pas de spam)
        emergencyStop();
        failsafeActive = true;
        if (!DEBUG_NONE) Serial.println("[FAILSAFE] RF lost -> stop");
      }
    }
  }
}
