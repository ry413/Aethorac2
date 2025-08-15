import serial
from serial.tools import list_ports
import time

# 设置串口参数

def select_serial_port():
    ports = list_ports.comports()
    if not ports:
        print("未检测到可用串口")
        exit(1)

    print("可用串口列表：")
    for i, p in enumerate(ports):
        print(f"  {i+1}. {p.device}  ({p.description})")

    idx = int(input("请选择串口编号: ")) - 1
    return ports[idx].device


SERIAL_PORT = select_serial_port()
BAUDRATE = 9600

# 数据字典
payload_dict = {
    # 7F01010FEF3C
    # 7F00010FEF0A
    "1": '7F 01 01 0F EF 3C BB 7E', # 60%
    "2": '7F 01 01 0F 00 0A 9A 7E', # 10%
    "3": '7F 01 01 0F AA 14 4E 7E', # 20%
    "11": '7F 00 01 0F EF 0A 88 7E',
    # "2": '7F 01 01 03 BF 3C 7F 7E',
    # "3": '7F 01 01 06 FE 3C C1 7E',
    "4": '7F 01 01 06 F7 3C BA 7E',
    "5": '7F 01 01 09 F7 3C BD 7E',
    "6": '7F 01 01 0A FE 3C C5 7E',
    "7": '7F 01 01 0A F7 3C BE 7E',
    
    "8": '7F 01 01 0F EF 14 93 7E',
    "9": '7F 01 01 0F DF 14 83 7E',

    # "0": '7F 00 00 00 FF 05 83 7E',     # 松开面板0
    # "1": '7F 00 00 00 FE 05 82 7E',     # 按住面板0的按键0
    # "4": '7F 00 00 00 EF 05 73 7E',     # 按住面板0的按键4
    # "5": '7F 00 00 00 DF 05 63 7E',     # 按住面板0的按键5    
    # "9": '7F 79 00 01 18 11 22 7E',     # 调光
    "10": '7F 80 16 01 03 00 19 7E',
    # "11": '7F 80 10 0D 00 00 1C 7E',

}


# 初始化串口
try:
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
    print(f"已打开串口 {SERIAL_PORT}，波特率 {BAUDRATE}")
except Exception as e:
    print(f"无法打开串口: {e}")
    exit(1)

# 主循环
try:
    while True:
        key = input(">").strip()
        if key.lower() == 'exit':
            break
        if key not in payload_dict:
            print("无效的编号，请重新输入。")
            continue
        
        hex_str = payload_dict[key]
        try:
            data_bytes = bytes.fromhex(hex_str)
        except ValueError:
            print(f"字典中编号 {key} 的格式不正确: {hex_str}")
            continue

        ser.write(data_bytes)
        print(f"已发送: {hex_str}")

except KeyboardInterrupt:
    print("\n用户终止程序。")

finally:
    ser.close()
    print("串口已关闭。")