import bluetooth
import RPi.GPIO as GPIO
import time
from threading import Thread

# --- Configuration GPIO ---
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

# Pins moteurs
MOTEUR_G_A = 17
MOTEUR_G_B = 27
MOTEUR_D_A = 22
MOTEUR_D_B = 23

# Pins capteurs
WATER_SENSOR_PIN    = 5
PRESENCE_SENSOR_PIN = 6

# Setup
GPIO.setup(MOTEUR_G_A, GPIO.OUT)
GPIO.setup(MOTEUR_G_B, GPIO.OUT)
GPIO.setup(MOTEUR_D_A, GPIO.OUT)
GPIO.setup(MOTEUR_D_B, GPIO.OUT)
GPIO.setup(WATER_SENSOR_PIN,    GPIO.IN)
GPIO.setup(PRESENCE_SENSOR_PIN, GPIO.IN)

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
    GPIO.output(MOTEUR_G_A, GPIO.LOW)
    GPIO.output(MOTEUR_G_B, GPIO.HIGH)
    GPIO.output(MOTEUR_D_A, GPIO.HIGH)
    GPIO.output(MOTEUR_D_B, GPIO.LOW)

def tourner_droite():
    GPIO.output(MOTEUR_G_A, GPIO.HIGH)
    GPIO.output(MOTEUR_G_B, GPIO.LOW)
    GPIO.output(MOTEUR_D_A, GPIO.LOW)
    GPIO.output(MOTEUR_D_B, GPIO.HIGH)

def stopper():
    GPIO.output(MOTEUR_G_A, GPIO.LOW)
    GPIO.output(MOTEUR_G_B, GPIO.LOW)
    GPIO.output(MOTEUR_D_A, GPIO.LOW)
    GPIO.output(MOTEUR_D_B, GPIO.LOW)

# --- Serveur Bluetooth ---
class BluetoothServer:
    def __init__(self):
        self.server_socket = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
        self.server_socket.bind(("", bluetooth.PORT_ANY))
        self.server_socket.listen(1)
        self.port = self.server_socket.getsockname()[1]
        self.running = True

    def start(self):
        print(f"Bluetooth serveur sur port {self.port}")
        bluetooth.advertise_service(
            self.server_socket, "SkimBot",
            service_id="SkimBotService",
            service_classes=[bluetooth.SERIAL_PORT_CLASS],
            profiles=[bluetooth.SERIAL_PORT_PROFILE]
        )
        while self.running:
            try:
                client_socket, client_info = self.server_socket.accept()
                print(f"Connexion: {client_info}")
                Thread(target=self.handle_client, args=(client_socket,)).start()
            except:
                pass

    def handle_client(self, client_socket):
        try:
            while True:
                data = client_socket.recv(1024).decode()
                if data:
                    self.process_command(data, client_socket)
        except:
            pass
        finally:
            stopper()
            client_socket.close()

    def process_command(self, command, client_socket):
        cmd = command.strip().upper()
        print(f"Commande reçue : {cmd}")

        if cmd == "AVANCER":
            avancer()
            client_socket.send(b"MOTEUR:AVANCER\n")
        elif cmd == "RECULER":
            reculer()
            client_socket.send(b"MOTEUR:RECULER\n")
        elif cmd == "GAUCHE":
            tourner_gauche()
            client_socket.send(b"MOTEUR:GAUCHE\n")
        elif cmd == "DROITE":
            tourner_droite()
            client_socket.send(b"MOTEUR:DROITE\n")
        elif cmd == "STOP":
            stopper()
            client_socket.send(b"MOTEUR:STOP\n")
        elif cmd == "EAU":
            water = GPIO.input(WATER_SENSOR_PIN)
            client_socket.send(f"EAU:{water}\n".encode())
        elif cmd == "PRESENCE":
            presence = GPIO.input(PRESENCE_SENSOR_PIN)
            client_socket.send(f"PRESENCE:{presence}\n".encode())
        elif cmd == "STATUS":
            water    = GPIO.input(WATER_SENSOR_PIN)
            presence = GPIO.input(PRESENCE_SENSOR_PIN)
            client_socket.send(f"EAU:{water},PRESENCE:{presence}\n".encode())

# --- Lancement ---
if __name__ == "__main__":
    try:
        print("Démarrage SkimBot...")
        server = BluetoothServer()
        server.start()
    except KeyboardInterrupt:
        stopper()
        GPIO.cleanup()
        print("Arrêt du SkimBot")
