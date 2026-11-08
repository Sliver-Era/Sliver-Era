#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
    /*
    日志配置桥接事件词汇
    日志配置事件类别与载荷字段名集中于此，
    加载与卸载标签沿用事件协议词汇（见 event_protocol）。
    */
    namespace log_event
    {
        //事件类别
        inline constexpr const char* category = "Log";

        // ———— 载荷字段名 ————

        //日志根目录
        inline constexpr const char* field_root = "root";
        //规则优先级模式
        inline constexpr const char* field_priority = "priority";
        //全局取反
        inline constexpr const char* field_verdict_invert = "verdict_invert";
        //输出流清单
        inline constexpr const char* field_streams = "streams";
        //流路径
        inline constexpr const char* field_path = "path";
        //流文件
        inline constexpr const char* field_file = "file";
        //文件许可表
        inline constexpr const char* field_files = "files";
        //目录许可表
        inline constexpr const char* field_dirs = "dirs";
        //级别许可表
        inline constexpr const char* field_levels = "levels";
    }
}
