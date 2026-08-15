#include "absolute_power.h"
#include <iostream>

/**
 * @brief 演示绝对功率结果结构体的创建、写入和 APF 读取方法。
 * 输入：无命令行参数。
 * @return 输出：程序正常结束时返回 0。
 */
int main() {
    absolute_power::EEG_Absolute_Power_SoA eeg_data(8); // 初始化 8 个通道

    // 写入数据：第 2 通道的 LO_ALPHA 功率
    eeg_data.at(absolute_power::LO_ALPHA, 2) = 15.6;
    
    // 读取数据：第 2 通道的 APF
    double apf = eeg_data.get_apf(2); 
    std::cout << "Channel 2 APF: " << apf << std::endl;
    return 0;
}
