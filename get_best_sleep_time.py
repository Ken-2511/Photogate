import time
import serial_control as sc

ser = sc.MySerial("COM6")

for message_length in range(10):
    for t in [0.01, 0.03, 0.1, 0.3, 0.5]:
        for i in range(10):
            message = f"{i} NXS @@@@{'h' * message_length};"
            ans = ser._send(message)
            time.sleep(t)
            print(t, message_length, ans, message)
        if input("continue") == "y":
            continue
        break

ser.close()