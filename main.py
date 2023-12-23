import tkinter as tk
from tkinter import ttk
import serial_control as sc
import threading
import time
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.backend_bases import key_press_handler
from matplotlib.figure import Figure
import numpy as np
from datetime import datetime

class PhotogateUnion:
    def __init__(self, serial: sc.MySerial):
        self.photogate_ids = [str(i) for i in range(10)]
        self.ser = serial
        self.run = True
        self.photogate_seen_times = [0 for _ in self.photogate_ids]
        self.photogate_start_times = [0 for _ in self.photogate_ids]
        self.photogate_frames = []
        self.word2byte_dir = {
            "done": b'^\\Ty',
            "seen": b'myy7',
            "----": b'@@@@',
            "errr": b'yPPP',
            "0": b'   ?',
            "1": b'   \x06',
            "2": b'   [',
            "3": b'   O',
            "4": b'   f',
            "5": b'   m',
            "6": b'   }',
            "7": b'   \x07',
            "8": b'   \x7f',
            "9": b'   o'
        }

    def close(self):
        self.run = False

    class Checker:
        # 这个类里面包含各种检测回复内容是否达标的函数
        @staticmethod
        def is_ok_checker(ans: str):
            return ans == "ok"

        @staticmethod
        def is_time_checker(ans: str):
            # 检查ans是否是不为零的数字
            try:
                int(ans)
                return True
            except ValueError:
                return False

    def broadcast(self, message: str | bytes, checker=Checker.is_ok_checker, repeats=3) -> list:
        # 广播某条消息，要求这条消息应当回复的内容使用checker进行审查
        # 最终回复一个列表，列表里是所有光电门的回复信息。如果回复不符合要求，对应的位置就是None
        answer = [None for _ in self.photogate_ids]
        for _ in range(repeats):  # 重复发送n次
            for idx, i in enumerate(self.photogate_ids):
                if not self.run:  # 这里时刻监控运行状态，如果程序退出了，就直接返回
                    return answer
                if answer[idx] is not None:
                    continue
                if isinstance(message, bytes):
                    mess = f"{i} ".encode() + message
                else:
                    mess = f"{i} {message}"
                ans = self.ser.send(mess)
                if checker(ans):
                    answer[idx] = ans
        return answer

    def broadcast_list(self, messages: list, checker=Checker.is_ok_checker, repeats=3) -> list:
        # 广播某条消息，要求这条消息应当回复的内容使用checker进行审查
        # 最终回复一个列表，列表里是所有光电门的回复信息。如果回复不符合要求，对应的位置就是None
        answer = [None for _ in self.photogate_ids]
        for _ in range(repeats):  # 重复发送n次
            for idx, i in enumerate(self.photogate_ids):
                if not self.run:  # 这里时刻监控运行状态，如果程序退出了，就直接返回
                    return answer
                if answer[idx] is not None:
                    continue
                mess = messages[idx]
                if isinstance(mess, bytes):
                    mess = f"{i} ".encode() + mess
                else:
                    mess = f"{i} {mess}"
                ans = self.ser.send(mess)
                if checker(ans):
                    answer[idx] = ans
        return answer

    def broadcast_non_block(self, *args):
        threading.Thread(target=self.broadcast, args=args).start()

    def broadcast_list_non_block(self, *args):
        threading.Thread(target=self.broadcast_list, args=args).start()

    def laser_on(self):
        # 打开所有激光
        self.broadcast_non_block("LSR ON;")

    def laser_off(self):
        # 关掉所有激光
        self.broadcast_non_block("LSR OFF;")

    def monitor_on(self):
        # 开始监控
        self.broadcast_non_block("MNT ON;")

    def monitor_off(self):
        # 停止监控
        self.broadcast_non_block("MNT OFF;")

    def show_id(self):
        # 让所有光电门在数码管上显示各自的ID
        messages = [f"NXS ".encode() + self.word2byte_dir[i] + b';' for i in self.photogate_ids]
        self.broadcast_list_non_block(messages)

    def show_voltage(self):
        # 获取光电门的电压信息并显示
        voltages = self.broadcast("TLV;", self.Checker.is_time_checker)
        for i in range(len(voltages)):
            if voltages[i] is None:
                voltages[i] = 0
        messages = [f"NXN {str(i).rjust(4, '0')};" for i in voltages]
        self.broadcast_list_non_block(messages)

    def get_time(self):
        # 获取光电门看到的时间
        # 前提是monitor已经seen了
        self.adjust_time()
        answer = self.broadcast("TLTM;", self.Checker.is_time_checker, 5)
        answer = [int(i) for i in answer]  # 字符串转数字
        for idx, i in enumerate(self.photogate_ids):
            num = self.photogate_time_to_abs_time(idx, answer[idx])
            self.photogate_seen_times[idx] = num
        # make the times start from 0
        min_time = min(self.photogate_seen_times)
        for i in range(len(self.photogate_seen_times)):
            self.photogate_seen_times[i] -= min_time
        for i in range(len(self.photogate_seen_times)):
            self.photogate_frames[i].set_seen_time(self.photogate_seen_times[i])

    def show_time(self):
        messages = ["0000" for _ in self.photogate_ids]
        for i in range(len(self.photogate_ids)):
            seen_time = self.photogate_seen_times[i]
            if seen_time is None:
                continue
            seen_time //= 10000
            messages[i] = "NXN " + str(seen_time).rjust(4, "0") + ";"
            print(messages[i])
        self.broadcast_list_non_block(messages)

    def adjust_time(self):
        # 和光电门校对时间。这个时间是主控时间和光电门启动时间之差，微秒数，一般误差在100μs左右
        answer = self.broadcast("ADJT;", self.Checker.is_time_checker, 5)
        answer = [int(i) for i in answer]
        self.photogate_start_times = answer.copy()

    def photogate_time_to_abs_time(self, idx: int, p_time: int):
        return self.photogate_start_times[idx] + p_time

    def change_brightness(self, brightness: int):
        message = f"NXB {brightness};"
        self.broadcast_non_block(message)

    def debug_laser_on(self):
        self.broadcast_non_block("DBL ON;")

    def debug_laser_off(self):
        self.broadcast_non_block("DBL OFF;")

    class PhotogateFrame:
        def __init__(self, master, idx, i):
            self.master = master
            self.index = idx
            self.i = i
            self.fp = ttk.Frame(self.master)  # 针对于每个photogate的小frame
            self.fp.grid(row=idx, column=0, sticky=tk.W)
            self.font = tk.font.Font(size=11)
            # 关于光电门基本信息
            self.label_str_var = tk.StringVar(value=f"No.{i}: pos:")
            self.label = ttk.Label(self.fp, textvariable=self.label_str_var, font=self.font)
            self.label.grid(row=0, column=0)
            # 关于位置
            # 创建一个Entry小部件，并添加数字验证
            def validate_input(p):
                # 验证输入是否是数字
                if p == "":
                    return True
                try:
                    float(p)
                    return True
                except ValueError:
                    return False
            validate = root.register(validate_input)
            self.entry = tk.Entry(self.fp, validate="key", validatecommand=(validate, "%P"), width=5, font=self.font)
            self.entry.insert(0, str(self.index * 8))
            self.entry.grid(row=0, column=1)
            # 关于时间
            self.time_str_var = tk.StringVar(value=f"Haven't seen")
            self.time = ttk.Label(self.fp, textvariable=self.time_str_var, font=self.font)
            self.time.grid(row=0, column=2)

        def get_position(self):
            return float(self.entry.get())

        def set_seen_time(self, t):
            self.time_str_var.set(f"Seen time: {t / 1000000}")

    def compose_photogates(self):
        for idx, i in enumerate(pu.photogate_ids):
            fp = self.PhotogateFrame(f_photogate, idx, i)
            self.photogate_frames.append(fp)

class BackgroundRefreshService:
    all_services = []

    # 这是关于后台程序的类
    def __init__(self, loop_gen, sleep_time):
        """
        关于后台程序的类，在后台运行，程序结束后自动终止运行。
        后台程序应当随时查看self.run变量，run如果是False的话，就需要结束运行了。
        @param loop_gen: 这个参数应当是一个generator，生成一个可迭代对象。每一次迭代时都会有一定等待时间，以此减少CPU开支
        """
        self.run = False
        self.thread = threading.Thread(target=self.loop_caller)
        # loop_gen是后台程序的核心代码。写成这样是为了随时能回到主程序。
        # 每进行完一步都要yield一次，这样的话就会回到loop_caller中等待一定时间，然后继续执行
        self.loop_gen = loop_gen
        self.sleep_time = sleep_time

    def loop_caller(self):
        while self.run:
            for _ in self.loop_gen():
                if not self.run:
                    return
            time.sleep(self.sleep_time)

    def start(self):
        self.run = True
        BackgroundRefreshService.all_services.append(self)
        self.thread.start()

    @staticmethod
    def end():
        for service in BackgroundRefreshService.all_services:
            service.run = False

def compose_control():
    b_lsr_on = ttk.Button(f_control, style="theme.TButton", text="Turn on laser", command=pu.laser_on)
    b_lsr_on.grid(row=0, column=0, padx=5, pady=5)
    b_lsr_off = ttk.Button(f_control, style="theme.TButton", text="Turn off laser", command=pu.laser_off)
    b_lsr_off.grid(row=0, column=1, padx=5, pady=5)
    b_show_id = ttk.Button(f_control, style="theme.TButton", text="Show ID", command=pu.show_id)
    b_show_id.grid(row=1, column=0, padx=5, pady=5)
    b_show_voltage = ttk.Button(f_control, style="theme.TButton", text="Show Voltage", command=pu.show_voltage)
    b_show_voltage.grid(row=1, column=1, padx=5, pady=5)
    b_show_word = ttk.Button(f_control, style="theme.TButton", text="Monitor On", command=pu.monitor_on)
    b_show_word.grid(row=2, column=0, padx=5, pady=5)
    b_show_word = ttk.Button(f_control, style="theme.TButton", text="Monitor Off", command=pu.monitor_off)
    b_show_word.grid(row=2, column=1, padx=5, pady=5)
    def get_time():
        threading.Thread(target=pu.get_time).start()
    b_show_word = ttk.Button(f_control, style="theme.TButton", text="Get Time", command=get_time)
    b_show_word.grid(row=3, column=0, padx=5, pady=5)
    b_show_word = ttk.Button(f_control, style="theme.TButton", text="Show Time", command=pu.show_time)
    b_show_word.grid(row=3, column=1, padx=5, pady=5)
    b_show_word = ttk.Button(f_control, style="theme.TButton", text="Debug Laser", command=pu.debug_laser_on)
    b_show_word.grid(row=4, column=0, padx=5, pady=5)
    b_show_word = ttk.Button(f_control, style="theme.TButton", text="Cease Debug", command=pu.debug_laser_off)
    b_show_word.grid(row=4, column=1, padx=5, pady=5)

def compose_figure():
    fig = Figure(figsize=(5, 4), dpi=100)
    t = np.arange(0, 3, .01)
    subplot = fig.add_subplot(111)
    subplot.plot(t, 2 * np.sin(2 * np.pi * t))

    canvas = FigureCanvasTkAgg(fig, master=f_figure)  # A tk.DrawingArea.
    canvas.draw()
    canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)

    toolbar = NavigationToolbar2Tk(canvas, f_figure)
    toolbar.update()
    canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)

    def on_key_press(event):
        print("you pressed {}".format(event.key))
        key_press_handler(event, canvas, toolbar)

    canvas.mpl_connect("key_press_event", on_key_press)

    def change_plot():
        x = [float(i.get_position()) for i in pu.photogate_frames]
        t = pu.photogate_seen_times
        t = [round(i/1000000, 4) for i in t]
        subplot.clear()
        subplot.plot(t, x)
        subplot.scatter(t, x, color="orange")
        for i in range(len(x)):
            subplot.annotate(f"({t[i]},{x[i]})", (t[i], x[i]), textcoords="offset points", xytext=(35, -5), ha="center")
        canvas.draw()

    button = tk.Button(master=f_figure, text="change plot", command=change_plot)
    button.pack(side=tk.BOTTOM)

def compose_command():
    listbox = tk.Listbox(f_command)
    listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    scrollbar = ttk.Scrollbar(f_command)
    scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    # bind the commands
    scrollbar.config(command=listbox.yview)
    listbox.config(yscrollcommand=scrollbar.set)
    # 设置字体大小
    font = tk.font.Font(size=11)
    listbox.config(font=font)
    # 执行后台程序
    cmd_count = 0
    cmd_send_recv = True  # The next to get. True for send, False for receive

    def cmd_loop_gen():
        # 这个函数用来不断检测serial的conversation里是否有新内容，并将其显示到command line中
        nonlocal cmd_count, cmd_send_recv
        if cmd_send_recv:
            if cmd_count < ser.conv_length:
                text = ser.conversations[cmd_count][0]  # 这是对话内容
                now = datetime.now()
                now_str = f"{now.hour}:{now.minute}:{now.second}"
                listbox.insert(tk.END, f"{cmd_count} {now_str} -> {text}")
                listbox.yview('moveto', '1.0')
                cmd_send_recv = False
                listbox.update_idletasks()
                yield None
        else:
            if len(ser.conversations[cmd_count]) == 2:
                text = ser.conversations[cmd_count][1]
                now = datetime.now()
                now_str = f"{now.hour}:{now.minute}:{now.second}"
                listbox.insert(tk.END, f"{cmd_count} {now_str} -> {text}")
                listbox.itemconfig(cmd_count * 2 + 1, {'bg': '#D0D0D0'})
                listbox.yview('moveto', '1.0')
                cmd_send_recv = True
                cmd_count += 1
                listbox.update_idletasks()
                yield None

    bsc_cmd_line = BackgroundRefreshService(cmd_loop_gen, 0.1)
    bsc_cmd_line.start()

def compose_status():
    status_str_var = tk.StringVar()
    font = tk.font.Font(size=16)
    status_label = ttk.Label(f_status, textvariable=status_str_var, foreground="#FF4040", font=font)
    status_label.pack()
    last_message = ""

    def status_loop_gen():
        # 获取还没发送的消息的数量并显示
        nonlocal last_message
        num_awaiting = ser.conv_length - ser.index
        # 编辑文字
        if num_awaiting == 0:
            message = "Idle"
        elif num_awaiting == 1:
            message = "Awaiting for at least 1 command..."
        else:
            message = f"Awaiting for at least {ser.conv_length - ser.index} commands..."
        # 更新文字
        if last_message != message and root.winfo_exists():
            status_str_var.set(message)
            f_status.update_idletasks()
        last_message = message
        yield None

    brs_status = BackgroundRefreshService(status_loop_gen, 0.1)
    brs_status.start()

# def cbb_on_select(event):
#     value = cbb_port.get()
#     print(value)

ser = sc.get_recommend_port()
if ser is None:
    import win32api
    import win32con
    import sys
    import win32api
    import win32con
    import serial
    # 创建消息框
    result = win32api.MessageBox(0,
                                 "Didn't find a valid port. Select the port manually?",
                                 "Photogate Upper Computer",
                                 win32con.MB_YESNO)
    # 根据用户的选择进行相应的操作
    if result != win32con.IDYES:
        sys.exit()
    # 此时需要用户手动输入一个port
    def on_ok_button_click():
        global selected_port
        selected_port = combo_box.get()  # 获取用户选择的串口
        root.destroy()  # 关闭窗口
    # 创建主窗口
    root = tk.Tk()
    root.title("选择串口")

    # 创建ComboBox并设置选项
    port_list = sc.get_available_ports()  # 你的串口列表
    selected_port = port_list[0]
    combo_box = ttk.Combobox(root, values=port_list)
    combo_box.set(port_list[0])  # 设置默认选择
    combo_box.pack(padx=20, pady=10)

    # 创建确定按钮
    ok_button = tk.Button(root, text="Confirm", command=on_ok_button_click)
    ok_button.pack(pady=10)

    # 将窗口居中放置
    width, height = 200, 100
    x, y = (root.winfo_screenwidth() - width) // 2, (root.winfo_screenheight() - height) // 2
    root.geometry(f"{width}x{height}+{x}+{y}")

    # 运行主循环
    root.mainloop()

    # 在窗口关闭后，你可以通过 selected_port 变量来获取用户的选择
    try:
        ser = sc.MySerial(selected_port)
    except serial.SerialException:
        win32api.MessageBox(0, "Failed to open this port", "Photogate Upper Computer", win32con.MB_OK)
        sys.exit()

pu = PhotogateUnion(ser)

root = tk.Tk()
root.title("Photogate")
# root.geometry("720x540")
root.columnconfigure(0, weight=1)
root.columnconfigure(1, weight=2)

def _quit():
    root.quit()  # stops mainloop
    root.destroy()  # this is necessary on Windows to prevent
    # Fatal Python Error: PyEval_RestoreThread: NULL tstate

root.protocol("WM_DELETE_WINDOW", _quit)

# 定义风格
theme = ttk.Style()
theme.configure("theme.TFrame")
# theme.configure("theme.TButton", foreground="#cc8888", background="#cc8888")
# theme.configure("theme.TLabel", foreground="#FFFFFF", background="#202020")

# 定义各种各样的窗口
# status 是显示目前光电门状态的区域
f_status = ttk.LabelFrame(root, text="Status", style="theme.TFrame")
f_status.grid(row=0, column=0, columnspan=2, padx=10, pady=10, sticky=tk.W)
compose_status()
# control 是左边的用于控制光电门的区域
f_control = ttk.LabelFrame(root, text="Control", style="theme.TFrame")
f_control.grid(row=1, column=0, padx=10, pady=10)
compose_control()
# photogates 是左边的用于展示每个光电门状态的区域
f_photogate = ttk.LabelFrame(root, text="Photogates", style="theme.TFrame")
f_photogate.grid(row=2, column=0, padx=10, pady=10)
pu.compose_photogates()
# figure 是右边的用于绘图的区域
f_figure = ttk.Frame(root, style="theme.TFrame")
f_figure.grid(row=1, column=1, rowspan=2, padx=10, pady=10)
compose_figure()
# 底部有关命令的窗口
f_command = ttk.Frame(root, style="theme.TFrame")
f_command.grid(row=3, column=0, columnspan=2, sticky=tk.NSEW)
compose_command()

# 在窗口下放置更小的模块

# cbb_port_opts = sc.get_available_ports()
# cbb_port = ttk.Combobox(f_control, values=cbb_port_opts)
# cbb_port.grid(row=5, column=0, padx=5, pady=5)
# cbb_port.bind("<<ComboboxSelected>>", cbb_on_select)


# about the scrolling command lines


try:
    root.mainloop()
finally:
    # 主程序结束后也要结束后台程序
    pu.close()
    BackgroundRefreshService.end()
    ser.close()
