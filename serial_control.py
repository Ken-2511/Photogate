import serial
import serial.tools.list_ports
import time
import threading


class MySerial(serial.Serial):
    def __init__(self, port: str, baudrate=9600, timeout=0):
        super().__init__(port=port, baudrate=baudrate, timeout=timeout)
        self.conversations = [["", ""]]
        self.conv_length = 1
        self.index = 1
        self.send_loop_run = True       # 控制后台发送消息的函数的运行的变量
        self.send_loop_thread = threading.Thread(target=self._send_loop)
        self.send_loop_thread.start()

    def idle(self):
        """发送消息的现在是否空闲"""
        return len(self.conversations[-1]) == 2

    def get_sleep_time(self, send_message, receive_message) -> float:
        total_length = len(send_message) + len(receive_message) + 20
        calculated_sleep_time = total_length * 0.01
        sleep_time = min(0.5, max(0.02, calculated_sleep_time))
        # print(total_length, calculated_sleep_time, sleep_time)
        return sleep_time

    def _send_loop(self):
        while self.send_loop_run:
            if self.conv_length <= self.index:
                time.sleep(0.001)
                continue
            conversation = self.conversations[self.index]
            result = self._send(conversation[0])
            conversation.append(result)
            self.index += 1
            time.sleep(self.get_sleep_time(*conversation))

    def send(self, message: str | bytes):
        conversation = [message]
        self.conversations.append(conversation)
        self.conv_length += 1
        while len(conversation) != 2:
            time.sleep(0.001)
        return conversation[1]

    def _send(self, message: str | bytes):
        # 发送消息并等待回复
        while True:                     # 先把消息清空
            temp = self.read()
            if temp == b'':
                break
        # print(type(message), message)
        if isinstance(message, str):
            self.write(message.encode())    # 发送消息
        else:
            self.write(message)
        ans = bytes()                   # 等待消息传送过来
        t0 = time.time()
        while time.time() - t0 < 0.5:
            ans = self.read()
            if ans != b'':
                break
        t0 = time.time()                # 等待消息传完
        while time.time() - t0 < 0.5:
            c = self.read()
            if c == b'\r':
                return ans.decode()
            if c == b'':
                time.sleep(0.01)
                c = self.read()
                if c == b'':
                    return ans.decode()
            ans += c
        return "receive failed"

    def close(self):
        self.send_loop_run = False
        self.send_loop_thread.join()
        super().close()


def get_available_ports():
    ans = ["(None)"]
    for port in serial.tools.list_ports.comports():
        ans.append(port.device)
    return ans


def get_recommend_port():
    for port in get_available_ports():
        # 先尝试能不能打开port
        try:
            ser = MySerial(port)
        except serial.SerialException:
            continue
        # 清空buffer
        while ser.read():
            pass
        # 测试是否有相应，如果有就说明是想要的端口
        # 不知道为什么前几次发送消息都不会回复。。。这个问题懒得修复了，所以就多发送几次
        for _ in range(10):
            ans = ser._send("Are you ok;")
            if ans == "Yes":
                return ser
            time.sleep(0.1)
        # 如果不是想要的端口，要记得关掉它
        ser.close()
    return None


if __name__ == '__main__':
    ser = get_recommend_port()
    print(ser)
    ser.close()
