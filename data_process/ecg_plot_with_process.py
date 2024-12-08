import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt, iirnotch
from scipy.fftpack import fft, fftfreq
from scipy.signal import find_peaks

def butter_bandpass(lowcut, highcut, fs, order=4):
    """
    创建带通滤波器的系数
    :param lowcut: 低截止频率
    :param highcut: 高截止频率
    :param fs: 采样频率
    :param order: 滤波器阶数
    :return: 滤波器系数 b, a
    """
    nyquist = 0.5 * fs
    low = lowcut / nyquist
    high = highcut / nyquist
    b, a = butter(order, [low, high], btype='band')
    return b, a

def apply_bandpass_filter(data, lowcut, highcut, fs, order=2):
    """
    滤波处理函数
    :param data: 原始信号
    :param lowcut: 低截止频率
    :param highcut: 高截止频率
    :param fs: 采样频率
    :param order: 滤波器阶数
    :return: 滤波后的信号
    """
    b, a = butter_bandpass(lowcut, highcut, fs, order=order)
    y = filtfilt(b, a, data)
    return y

def apply_notch_filter(data, f0, fs, Q=30):
    """
    应用陷波滤波器以去除指定频率的干扰
    :param data: 输入信号
    :param f0: 要去除的频率（例如 50 Hz 工频）
    :param fs: 采样频率
    :param Q: 品质因数（Q 值越高，带宽越窄）
    :return: 滤波后的信号
    """
    # 创建陷波滤波器
    b, a = iirnotch(f0, Q, fs)
    # 应用滤波器
    y = filtfilt(b, a, data)
    return y

def calculate_fft(data, fs):
    """
    计算FFT并返回频率和功率谱
    :param data: 时域信号
    :param fs: 采样频率
    :return: 频率数组, 对应的功率谱（dB）
    """
    # 确保输入为 NumPy 数组
    data = np.asarray(data, dtype=float)
    
    n = len(data)
    freq = fftfreq(n, d=1/fs)[:n//2]  # 正频率部分
    fft_values = fft(data)[:n//2]
    magnitude = 20 * np.log10(np.abs(fft_values) + 1e-10)  # 转换为dB，避免log(0)
    return freq, magnitude

def plot_ecg_data_with_fft(file_path, output_path='ecg_plot.png'):
    # 读取CSV文件
    df = pd.read_csv(file_path, skiprows=1)
    
    # 转换时间和电压数据为浮点型
    time = df['Time(ms)'].astype(float).to_numpy() / 1000  # 转换为秒
    voltage = df['Voltage(V)'].astype(float).to_numpy()  # 转换为 NumPy 数组
    fs = 1000  # 采样率 1000Hz

    # Step 1: 先应用 50 Hz 陷波滤波器
    voltage = apply_notch_filter(voltage, f0=50, fs=fs, Q=30)

    # 原始信号的频域图像
    freq, magnitude = calculate_fft(voltage, fs)

    # Step 2: 再应用带通滤波器
    filtered_voltage = apply_bandpass_filter(voltage, 0.5, 40, fs)

    # 滤波后信号的频域图像
    filtered_freq, filtered_magnitude = calculate_fft(filtered_voltage, fs)

    # Step 3: R峰检测和心率计算
    try:
        peaks, _ = find_peaks(filtered_voltage, distance=fs/2.5, height=0.5)  # 设置合理的距离和高度阈值

        if len(peaks) == 0:
            raise ValueError("No peaks detected")

        rr_intervals = np.diff(peaks) / fs  # RR间期，单位为秒
        heart_rate = 60.0 / np.mean(rr_intervals)  # 计算平均心率
        print(f"Detected Heart Rate: {heart_rate:.2f} bpm")

    except Exception as e:
        print(f"Error during heart rate detection: {e}")
        heart_rate = None
        peaks = []

    # 创建图形
    plt.figure(figsize=(15, 6))

    # 原始信号时域图
    plt.subplot(2, 2, 1)
    plt.plot(time, voltage, 'b-', linewidth=1)
    plt.title('Original Signal (Time Domain)', fontsize=14)
    plt.xlabel('Time (s)', fontsize=12)
    plt.ylabel('Voltage (V)', fontsize=12)
    plt.grid()

    # 原始信号频域图
    plt.subplot(2, 2, 2)
    plt.plot(freq, magnitude, 'r-', linewidth=1)
    plt.title('Original Signal (Frequency Domain)', fontsize=14)
    plt.xlabel('Frequency (Hz)', fontsize=12)
    plt.ylabel('Magnitude (dB)', fontsize=12)
    plt.grid()

    # 滤波后信号时域图
    plt.subplot(2, 2, 3)
    plt.plot(time, filtered_voltage, 'g-', linewidth=1)
    plt.title('Filtered Signal (Time Domain)', fontsize=14)
    plt.xlabel('Time (s)', fontsize=12)
    plt.ylabel('Voltage (V)', fontsize=12)
    plt.grid()

    # 滤波后信号频域图
    plt.subplot(2, 2, 4)
    plt.plot(filtered_freq, filtered_magnitude, 'm-', linewidth=1)
    plt.title('Filtered Signal (Frequency Domain)', fontsize=14)
    plt.xlabel('Frequency (Hz)', fontsize=12)
    plt.ylabel('Magnitude (dB)', fontsize=12)
    plt.grid()

    plt.tight_layout()
    plt.savefig(output_path, dpi=200, bbox_inches='tight')

    # 心率探测图
    plt.figure(figsize=(10, 4))
    plt.plot(time, filtered_voltage, label='Filtered Signal', color='green')

    if len(peaks) > 0:
        plt.plot(time[peaks], filtered_voltage[peaks], 'rx', label='Detected Peaks')

    if heart_rate is not None:
        plt.title(f'ECG Signal with Detected Peaks (Heart Rate: {heart_rate:.2f} bpm)')
    else:
        plt.title('ECG Signal with Detected Peaks (No Peaks Detected)')

    plt.xlabel('Time [s]')
    plt.ylabel('Voltage [V]')
    plt.legend()
    plt.grid()
    plt.tight_layout()
    plt.savefig(output_path.replace('.png', '_peaks.png'), dpi=200)
    plt.close()

# 使用示例
if __name__ == "__main__":
    file_path = "ecg_example_4.csv"
    plot_ecg_data_with_fft(file_path, file_path + "_plot_processed.png")