#ifndef INDICATOR_H
#define INDICATOR_H

#include <vector>
#include <unordered_set>
#include <bits/std_function.h>

// 一个动作组完全执行完后, 一次性更新指示灯
class IndicatorHolder {
public:
    using FunctionType = std::function<void()>;

    // 获取单例实例
    static IndicatorHolder& getInstance() {
        static IndicatorHolder instance;
        return instance;
    }

    // 添加函数
    void addFunction(FunctionType func, const void* panelPtr);

    // 调用所有函数并清空
    void callAllAndClear();

private:
    std::vector<FunctionType> functions;
    std::unordered_set<const void*> registeredPanels;

    // 私有化构造函数和赋值运算符，确保单例
    IndicatorHolder() = default;
    IndicatorHolder(const IndicatorHolder&) = delete;
    IndicatorHolder& operator=(const IndicatorHolder&) = delete;
};

#endif
