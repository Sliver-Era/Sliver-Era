#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

//通用算法模块
namespace engine
{
    //辅助工具命名空间
    namespace detail
    {
        //浮点是否有限
        inline bool finite_check(float value)
        {
            return std::isfinite(value);
        }

        //浮点是否落在 [0, 1]
        inline bool unit_interval_check(float value)
        {
            return finite_check(value) && value >= 0.0f && value <= 1.0f;
        }

        //浮点是否落在 [min, max]
        inline bool range_check(float value, float min, float max)
        {
            return finite_check(value) && value >= min && value <= max;
        }
    }
}
