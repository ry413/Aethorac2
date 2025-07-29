from prompt_toolkit import PromptSession
from prompt_toolkit.patch_stdout import patch_stdout
from prompt_toolkit.completion import WordCompleter
import serial
from serial.tools import list_ports
import threading
import time


# === 配置区域 ===
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

COMMANDS = WordCompleter(
    [
        "oracle",
        "exit",
        "ora",
        "cra",
        "round",
        "net",
        "wifi",
        "ether",
        "ls",
        "e",
        "wifissid ",
        "wifipass ",
        "ver",
        "stmlogon",
        "stmlogoff",
        "rs485logon",
        "rs485logoff"
    ],
    ignore_case=True,
)

# === 全局状态 ===
curr_wifissid_packet_num = 0
curr_wifipass_packet_num = 0


# === 命令生成 ===
def build_command_by_parts(type_, param1, param2):
    header = [0x7F, 0x79]
    zero = 0x00
    body = [type_, zero, param1, param2]
    checksum = (sum(header + body)) & 0xFF
    return header + body + [checksum, 0x7E]


def build_chunked_commands(base_type, data_str):
    packets = []
    chunk_size = 3
    for i in range(0, len(data_str), chunk_size):
        chunk = [ord(c) for c in data_str[i : i + chunk_size]]
        while len(chunk) < 3:
            chunk.append(0xFF)
        type_ = base_type + (i // chunk_size)
        pkt = [0x7F, 0x79, type_] + chunk
        checksum = sum(pkt) & 0xFF
        pkt += [checksum, 0x7E]
        packets.append(pkt)
    if len(data_str) % 3 == 0:
        type_ = base_type + (len(data_str) // 3)
        pkt = [0x7F, 0x79, type_, 0xFF, 0xFF, 0xFF]
        checksum = sum(pkt) & 0xFF
        pkt += [checksum, 0x7E]
        packets.append(pkt)
    return packets


def build_command(cmd_str):
    global curr_wifissid_packet_num, curr_wifipass_packet_num
    cmd_str = cmd_str.strip().lower()

    fixed_cmds = {
        "oracle": (0x01, 0x00, 0x00),
        "exit": (0x00, 0x00, 0x00),
        "ora": (0x05, 0x00, 0x01),
        "cra": (0x05, 0x00, 0x00),
        "round": (0x04, 0x00, 0x00),
        "net": (0x09, 0x00, 0x00),
        "wifi": (0x09, 0x00, 0x01),
        "ether": (0x09, 0x00, 0x02),
        "ver": (0x0A, 0x00, 0x00),
        "stmlogon": (0x0B, 0x00, 0x01),
        "stmlogoff": (0x0B, 0x00, 0x00),
        "rs485logon": (0x0B, 0x01, 0x01),
        "rs485logoff": (0x0B, 0x01, 0x00),
    }

    if cmd_str in fixed_cmds:
        return [build_command_by_parts(*fixed_cmds[cmd_str])]

    if cmd_str == "ls":
        print_help()
        return []

    if cmd_str.startswith("wifissid "):
        data = cmd_str[9:]
        if not data:
            raise ValueError("wifissid 后必须带参数")
        packets = build_chunked_commands(0x71, data)
        curr_wifissid_packet_num = len(packets)
        return packets

    if cmd_str.startswith("wifipass "):
        data = cmd_str[9:]
        if not data:
            raise ValueError("wifipass 后必须带参数")
        packets = build_chunked_commands(0x81, data)
        curr_wifipass_packet_num = len(packets)
        return packets

    if len(cmd_str) < 3:
        raise ValueError("命令格式不正确")

    action = cmd_str[:2]
    relay_str = cmd_str[2:]

    op_table = {
        "or": lambda num: (0x02, num, 0x01),
        "cr": lambda num: (0x02, num, 0x00),
        "od": lambda num: (0x03, num, 0x01),
        "cd": lambda num: (0x03, num, 0x00),
        "ih": lambda num: (0x06, num, 0x01),
        "il": lambda num: (0x06, num, 0x00),
        "oc": lambda num: (0x07, num, 0x00),
    }

    if action not in op_table:
        raise ValueError(f"不支持的操作: {cmd_str}")

    if not relay_str.isdigit():
        raise ValueError("继电器编号应为数字")

    relay_num = int(relay_str)
    if not (1 <= relay_num <= 255):
        raise ValueError("继电器编号应在 1~255 范围内")

    return [build_command_by_parts(*op_table[action](relay_num))]


# === 响应解析 ===
def parse_short_response(data):
    try:
        if len(data) != 8 or data[0] != 0x7F or data[1] != 0x79 or data[-1] != 0x7E:
            print("[!] 短响应包格式不正确")
            return

        if data[6] != (sum(data[:6]) & 0xFF):
            print("[!] 校验失败")
            return

        p1, p2, p3, p4 = data[2], data[3], data[4], data[5]

        responses = {
            (0x01, 0x00, 0x00, 0x00): "[√] 已进入测试模式",
            (0x00, 0x00, 0x00, 0x00): "[√] 已退出测试模式",
            (0x09, 0x01, 0x00, 0x00): "[√] 确认收到, 准备切换至 WiFi",
            (0x09, 0x01, 0xEE, 0x00): "[!] WiFi连接超时",
            (0x09, 0x02, 0x00, 0x00): "[√] 确认收到, 准备切换至 Ethernet",
            (0x79, 0x00, 0x00, 0x00): "[X] 未处于测试模式",
        }

        key = (p1, p2, p3, p4)
        if key in responses:
            print(f"ESP响应: {responses[key]}")
        elif 0x91 <= p1 <= 0x9F:
            print(f"ESP响应: [√] SSID包 {p1 - 0x90}/{curr_wifissid_packet_num} 确认")
        elif 0xA1 <= p1 <= 0xAF:
            print(f"ESP响应: [√] PASS包 {p1 - 0xA0}/{curr_wifipass_packet_num} 确认")
        elif p1 == 0x0A:
            print(f"ESP响应: [√] 固件版本: {p2}.{p3}.{p4}")
        else:
            print(f"[?] 未知响应: {p1:02X} {p2:02X} {p3:02X} {p4:02X}")
    except Exception as e:
        print(f"[!] 解析短响应失败: {e}")


def parse_received_data(data):
    try:
        if len(data) != 14 or data[0] != 0x7F or data[1] != 0x79 or data[-1] != 0x7E:
            print("[!] 网络状态包格式不正确")
            return

        if data[12] != (sum(data[:12]) & 0xFF):
            print("[!] 校验失败")
            return

        seq = int.from_bytes(data[3:7], byteorder="big")
        ip = ".".join(map(str, data[7:11]))
        net = {0x01: "WiFi", 0x02: "Ethernet", 0x00: "未知"}.get(
            data[11], f"未知类型({data[11]:02X})"
        )
        print(f"[√] 状态上报: 序号:{seq:08d} IP:{ip} 网络:{net}")

    except Exception as e:
        print(f"[!] 解析网络状态包失败: {e}")


# === 串口数据接收 ===
def receiver_thread(ser):
    WAIT_FOR_HEADER = 0
    RECEIVE_DATA = 1
    state = WAIT_FOR_HEADER

    current_frame = bytearray()
    MAX_FRAME_SIZE = 14

    while True:
        try:
            if ser.in_waiting:
                byte = ser.read(1)
                if not byte:
                    continue
                byte_val = byte[0]

                if state == WAIT_FOR_HEADER:
                    if byte_val == 0x7F:
                        current_frame = bytearray([byte_val])
                        state = RECEIVE_DATA
                    # else: 忽略无效首字节

                elif state == RECEIVE_DATA:
                    current_frame.append(byte_val)

                    if len(current_frame) == 2 and current_frame[1] != 0x79:
                        # 第二个字节不是0x79，说明包格式不对
                        state = WAIT_FOR_HEADER
                        continue

                    # 检查尾字节是否已到达
                    if byte_val == 0x7E:
                        length = len(current_frame)
                        if length == 8:
                            parse_short_response(current_frame)
                        elif length == 14:
                            parse_received_data(current_frame)
                        else:
                            print(
                                f"[!] 收到未知长度的帧 ({length}字节): {current_frame.hex()}"
                            )
                        state = WAIT_FOR_HEADER

                    elif len(current_frame) > MAX_FRAME_SIZE:
                        print(f"[!] 超过最大帧长，丢弃: {current_frame.hex()}")
                        state = WAIT_FOR_HEADER

        except Exception as e:
            print(f"[!] 接收线程出错: {e}")
            break


# === 命令帮助打印 ===
def print_help():
    print(
        """
可用指令:
  oracle          - 进入测试模式
  exit            - 退出测试模式
  orX / crX       - 打开/关闭继电器X(如 or1, cr6)
  odX / cdX       - 打开/关闭干接点输出X(如 od7, cd9)
  ihX / ilX       - 打开/关闭干接点输入X(如ih1, il2)
  ora / cra       - 全开/全关 所有继电器与干接点输出
  round           - 跑马灯测试
  net             - 请求当前网络信息
  wifi / ether    - 请求切换到 WiFi / Ethernet
  wifissid XXX    - 设置 WiFi SSID
  wifipass XXX    - 设置 WiFi 密码
  rs485log[on|off]- 开关485指令码打印
  stm32log[on|off]- 开关stm32指令吗打印
  ls              - 显示本帮助
  e               - 退出脚本
"""
    )


# === 主程序入口 ===
session = PromptSession()


def main():
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
    threading.Thread(target=receiver_thread, args=(ser,), daemon=True).start()
    print_help()

    with patch_stdout():
        while True:
            try:
                cmd = session.prompt("> ", completer=COMMANDS).strip()
                if cmd == "e":
                    return
                for pkt in build_command(cmd):
                    ser.write(bytearray(pkt))
                    time.sleep(0.3)
            except ValueError as ve:
                print("输入错误:", ve)
            except Exception as e:
                print(f"发生错误: {e}")

        ser.close()


if __name__ == "__main__":
    main()
