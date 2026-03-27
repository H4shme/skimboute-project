import RPi.GPIO as GPIO
import serial
import time

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

MOTEUR_G_A = 17
MOTEUR_G_B = 27
MOTEUR_D_A = 22
MOTEUR_D_B = 23
WATER_SENSOR_PIN    = 5
PRESENCE_SENSOR_PIN = 6

GPIO.setup(MOTEUR_G_A, GPIO.OUT)
GPIO.setup(MOTEUR_G_B, GPIO.OUT)
GPIO.setup(MOTEUR_D_A, GPIO.OUT)
GPIO.setup(MOTEUR_D_B, GPIO.OUT)
GPIO.setup(WATER_SENSOR_PIN,    GPIO.IN)
GPIO.setup(PRESENCE_SENSOR_PIN, GPIO.IN)

ser = serial.Serial('/dev/ttyAMA0', baudrate=9600, timeout=1)

def avancer():
    GPIO.output(MOTEUR_G_A, GPIO.HIGH); GPIO.output(MOTEUR_G_B, GPIO.LOW)
    GPIO.output(MOTEUR_D_A, GPIO.HIGH); GPIO.output(MOTEUR_D_B, GPIO.LOW)

def reculer():
    GPIO.output(MOTEUR_G_A, GPIO.LOW);  GPIO.output(MOTEUR_G_B, GPIO.HIGH)
    GPIO.output(MOTEUR_D_A, GPIO.LOW);  GPIO.output(MOTEUR_D_B, GPIO.HIGH)

def tourner_gauche():
    GPIO.output(MOTEUR_G_A, GPIO.LOW);  GPIO.output(MOTEUR_G_B, GPIO.HIGH)
    GPIO.output(MOTEUR_D_A, GPIO.HIGH); GPIO.output(MOTEUR_D_B, GPIO.LOW)

def tourner_droite():
    GPIO.output(MOTEUR_G_A, GPIO.HIGH); GPIO.output(MOTEUR_G_B, GPIO.LOW)
    GPIO.output(MOTEUR_D_A, GPIO.LOW);  GPIO.output(MOTEUR_D_B, GPIO.HIGH)

def stopper():
    GPIO.output(MOTEUR_G_A, GPIO.LOW); GPIO.output(MOTEUR_G_B, GPIO.LOW)
    GPIO.output(MOTEUR_D_A, GPIO.LOW); GPIO.output(MOTEUR_D_B, GPIO.LOW)

print("En attente de commandes Bluetooth...")
try:
    while True:
        if ser.in_waiting > 0:
            commande = ser.readline().decode('utf-8', errors='ignore').strip().upper()
            print(f"Commande reçue : {commande}")

            if commande == "AVANCER":
                avancer(); ser.write(b"MOTEUR:AVANCER\n")
            elif commande == "RECULER":
                reculer(); ser.write(b"MOTEUR:RECULER\n")
            elif commande == "GAUCHE":
                tourner_gauche(); ser.write(b"MOTEUR:GAUCHE\n")
            elif commande == "DROITE":
                tourner_droite(); ser.write(b"MOTEUR:DROITE\n")
            elif commande == "STOP":
                stopper(); ser.write(b"MOTEUR:STOP\n")
            elif commande == "EAU":
                water = GPIO.input(WATER_SENSOR_PIN)
                ser.write(f"EAU:{water}\n".encode())
            elif commande == "PRESENCE":
                presence = GPIO.input(PRESENCE_SENSOR_PIN)
                ser.write(f"PRESENCE:{presence}\n".encode())
            elif commande == "STATUS":
                water = GPIO.input(WATER_SENSOR_PIN)
                presence = GPIO.input(PRESENCE_SENSOR_PIN)
                ser.write(f"EAU:{water},PRESENCE:{presence}\n".encode())
            else:
                print("Commande inconnue")
                ser.write(b"ERREUR:COMMANDE_INCONNUE\n")

        time.sleep(0.1)

except KeyboardInterrupt:
    print("Arrêt du programme")
    stopper()
    GPIO.cleanup()
    ser.close()
