#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
    /*
    配置加载模块事件词汇
    配置加载器自身的事件类别、标签与载荷字段名集中于此，
    加载与卸载标签沿用事件协议词汇（见 event_protocol）。
    */
    namespace config_event
    {
        // ———— 本模块接收 ————

        //可信根目录类别
        inline constexpr const char* believed_root = "Believed_Root";
        //配置路由类别
        inline constexpr const char* route = "Route";
        //配置文件类别
        inline constexpr const char* config_file = "Config_File";

        // ———— 载荷字段名 ————

        //索引目录
        inline constexpr const char* field_route = "route";
        //配置目录
        inline constexpr const char* field_config = "config";
        //配置文件相对路径
        inline constexpr const char* field_config_path = "config_path";
        //投递事件类别（路由条目可覆盖）
        inline constexpr const char* field_event_category = "event_category";
        //投递事件标签（路由条目可覆盖）
        inline constexpr const char* field_event_tag = "event_tag";
        //配置文件清单
        inline constexpr const char* field_files = "files";
        //脏标记
        inline constexpr const char* field_dirty_mark = "dirty_mark";
        //路由条目集合
        inline constexpr const char* field_config_routes = "config_routes";
    }
}
