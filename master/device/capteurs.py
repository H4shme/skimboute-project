import spidev
import time

spi = spidev.SpiDev()
spi.open(0, 0)
spi.max_speed_hz = 1350000
SENSOR_PIN = 0  # Canal CH0 du MCP3008

def read_analog(channel):
    adc = spi.xfer2([1, (8 + channel) << 4, 0])
    return ((adc[1] & 3) << 8) + adc[2]

try:
    while True:
        liquid_level = read_analog(SENSOR_PIN)
        print(f"Niveau : {liquid_level}")
        time.sleep(0.1)
except KeyboardInterrupt:
    spi.close()
