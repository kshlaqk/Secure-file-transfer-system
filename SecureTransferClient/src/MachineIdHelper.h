#ifndef MACHINEIDHELPER_H
#define MACHINEIDHELPER_H

#include <QString>

/**
 * @brief 机器码获取辅助类
 * 
 * 用于获取Windows系统的唯一机器标识
 */
class MachineIdHelper
{
public:
    /**
     * @brief 获取本机唯一机器码
     * @return 机器码字符串（如果获取失败返回空字符串）
     */
    static QString getMachineId();
    
private:
    // Windows特定实现
    static QString getWindowsMachineId();
};

#endif // MACHINEIDHELPER_H
