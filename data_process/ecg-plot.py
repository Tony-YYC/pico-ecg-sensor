import pandas as pd
import matplotlib.pyplot as plt

def plot_ecg_data(file_path, output_path='ecg_plot.png'):
    # 读取CSV文件
    # skiprows=1 是因为第一行包含"Starting new capture..."这样的文本信息
    df = pd.read_csv(file_path, skiprows=1)
    
    # 创建图形和子图
    plt.figure(figsize=(15, 6))
    
    # 绘制心电图
    plt.plot(df['Time(ms)'], df['Voltage(V)'], 'b-', linewidth=1)
    
    # 设置图表标题和标签
    plt.title('ECG Signal', fontsize=14)
    plt.xlabel('Time (ms)', fontsize=12)
    plt.ylabel('Voltage (V)', fontsize=12)
    
    # 设置x轴主刻度间距为500ms
    plt.gca().xaxis.set_major_locator(plt.MultipleLocator(500))
    
    # 添加网格，但只显示主刻度的网格线
    plt.grid(True, which='major', linestyle='--', alpha=0.7)
    plt.grid(False, which='minor')  # 关闭次要网格线
    
    # 调整布局
    plt.tight_layout()
    
    # 保存图像，dpi=200
    plt.savefig(output_path, dpi=200, bbox_inches='tight')
    
    # 显示图形
    plt.show()

# 使用示例
if __name__ == "__main__":
    # 替换为你的CSV文件路径
    file_path = "ecg_example_4.csv"
    plot_ecg_data(file_path,file_path+"_plot.png")