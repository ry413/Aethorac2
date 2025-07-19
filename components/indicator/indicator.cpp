#include "indicator.h"

void IndicatorHolder::callAllAndClear() {
    for (const auto& func : functions) {
        func();
    }
    functions.clear();
    registeredPanels.clear();
}

void IndicatorHolder::addFunction(FunctionType func, const void* panelPtr) {
    if (registeredPanels.find(panelPtr) == registeredPanels.end()) {
        functions.push_back(func);
        registeredPanels.insert(panelPtr);
    }
}