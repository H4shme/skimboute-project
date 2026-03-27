import RPi.GPIO as GPIO
import serial
import time

# --- Configuration GPIO ---
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

# Pins moteurs
MOTEUR_G_A = 17  # Moteur gauche fil 1
MOTEUR_G_B = 27  # Moteur gauche fil 2
MOTEUR_D_A = 22  # Moteur droit fil 1
MOTEUR_D_B = 23  # Moteur droit fil 2

GPIO.setup(MOTEUR_G_A, GPIO.OUT)
GPIO.setup(MOTEUR_G_B, GPIO.OUT)
GPIO.setup(MOTEUR_D_A, GPIO.OUT)
GPIO.setup(MOTEUR_D_B, GPIO.OUT)

# --- Configuration Bluetooth ---
ser = serial.Serial('/dev/ttyAMA0', baudrate=9600, timeout=1)

# --- Fonctions moteurs ---
def avancer():
    GPIO.output(MOTEUR_G_A, GPIO.HIGH)
    GPIO.output(MOTEUR_G_B, GPIO.LOW)
    GPIO.output(MOTEUR_D_A, GPIO.HIGH)
    GPIO.output(MOTEUR_D_B, GPIO.LOW)

def reculer():
    GPIO.output(MOTEUR_G_A, GPIO.LOW)
    GPIO.output(MOTEUR_G_B, GPIO.HIGH)
    GPIO.output(MOTEUR_D_A, GPIO.LOW)
    GPIO.output(MOTEUR_D_B, GPIO.HIGH)

def tourner_gauche():
    # Moteur gauche recule, moteur droit avance
    GPIO.output(MOTEUR_G_A, GPIO.LOW)
    GPIO.output(MOTEUR_G_B, GPIO.HIGH)
    GPIO.output(MOTEUR_D_A, GPIO.HIGH)
    GPIO.output(MOTEUR_D_B, GPIO.LOW)

def tourner_droite():
    # Moteur gauche avance, moteur droit recule
    GPIO.output(MOTEUR_G_A, GPIO.HIGH)
    GPIO.output(MOTEUR_G_B, GPIO.LOW)
    GPIO.output(MOTEUR_D_A, GPIO.LOW)
    GPIO.output(MOTEUR_D_B, GPIO.HIGH)

def stopper():
    GPIO.output(MOTEUR_G_A, GPIO.LOW)
    GPIO.output(MOTEUR_G_B, GPIO.LOW)
    GPIO.output(MOTEUR_D_A, GPIO.LOW)
    GPIO.output(MOTEUR_D_B, GPIO.LOW)

# --- Boucle principale ---
print("En attente de commandes Bluetooth...")

try:
    while True:
        if ser.in_waiting > 0:
            commande = ser.readline().decode('utf-8').strip()
            print(f"Commande reçue : {commande}")

            if commande == "AVANCER":
                avancer()
            elif commande == "RECULER":
                reculer()
            elif commande == "GAUCHE":
                tourner_gauche()
            elif commande == "DROITE":
                tourner_droite()
            elif commande == "STOP":
                stopper()
            else:
                print("Commande inconnue")

        time.sleep(0.05)

except KeyboardInterrupt:
    print("Arrêt du programme")
    stopper()
    GPIO.cleanup()
    ser.close()
