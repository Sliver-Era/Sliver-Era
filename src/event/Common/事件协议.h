#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

//游戏引擎命名空间
namespace engine
{
    /*
    事件中转站协议词汇
    中转站自身的订阅登记、订阅者查询与配置投递约定集中于此，
    避免各模块各写一份协议字符串导致发送侧与接收侧漂移。
    */
    namespace event_protocol
    {
        // ———— 订阅登记协议 ————

        //订阅事件清单
        inline constexpr const char* subscribed_events = "Subscribed_Events";
        //订阅登记
        inline constexpr const char* add = "Add";
        //订阅注销
        inline constexpr const char* remove = "Remove";

        // ———— 订阅者查询协议 ————

        //订阅者查询
        inline constexpr const char* subscriber = "Subscriber";
        //订阅者存在性查询
        inline constexpr const char* check = "Check";
        //订阅者退出
        inline constexpr const char* detach = "Detach";
        //事件调用
        inline constexpr const char* call = "Call";
        //调用应答
        inline constexpr const char* response = "Response";

        // ———— 配置投递约定 ————

        //配置事件类别
        inline constexpr const char* config = "Config";
        //加载标签
        inline constexpr const char* load = "Load";
        //卸载标签
        inline constexpr const char* unload = "Unload";

        // ———— 保留标签 ————

        //中转站保留标签（该标签下的事件会被中转站直接丢弃）
        inline constexpr const char* all = "All";

        // ———— 载荷字段名 ————

        //目标对象名
        inline constexpr const char* field_object = "object";
        //权限密钥
        inline constexpr const char* field_acl_key = "acl_key";
        //订阅事件清单
        inline constexpr const char* field_subscribed_events = "subscribed_events";
        //订阅者存在性
        inline constexpr const char* field_subscribed = "subscribed";
        //事件类别名
        inline constexpr const char* field_category = "category";
        //事件标签名
        inline constexpr const char* field_tag = "tag";
    }
}
