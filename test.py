import matplotlib.pyplot as plt

x = [1, 2, 3, 4, 5]
y = [10, 20, 15, 30, 25]

# 绘制散点图
plt.scatter(x, y)

# 在每个点旁边显示坐标
for i, (xi, yi) in enumerate(zip(x, y)):
    plt.annotate(f'({xi}, {yi})', (xi, yi), textcoords='offset points', xytext=(0,10), ha='center')

plt.xlabel('X轴')
plt.ylabel('Y轴')
plt.title('散点图')
plt.show()
