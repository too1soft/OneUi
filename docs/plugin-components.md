# 开发和使用组件包

组件包把专用控件放在独立工程中。应用消费预编译 OneUI SDK，编译组件包和自己的页面，不需要修改框架。首个示例 `oneui.status` 提供带进度显示、键盘激活和无障碍信息的 `OneUI_Status`。

## 接入组件

使用包含组件协议的 OneUI ABI 46 SDK，并匹配包的编译器、架构、链接方式与运行库。安装包后，在页面编译之前声明组件：

```cmake
find_package(OneUI CONFIG REQUIRED)
include("${CMAKE_CURRENT_SOURCE_DIR}/.oneui/plugins/windows-modern-x86_64-shared/OneUIProjectPlugins.cmake")
include("${OneUI_DIR}/OneUIView.cmake")
add_executable(my_app main.cpp)
oneui_target_components(my_app REGISTRAR register_components
    COMPONENTS ${ONEUI_COMPONENT_TARGETS})
oneui_target_view(my_app NAME StatusDemo SOURCE Status.one VM_HEADER vm.h)
```

```html
<template view-model="StatusVM">
  <OneUI_Status :label="label" :value="value" @activate="activate" />
</template>
<style scoped>
OneUI_Status { width: 260px; height: 72px; color: #246bce; }
</style>
```

创建 `OneUiComponentRegistry`，调用生成的 `register_components(registry)`，再把它设置到 Mount。C++ 使用 `ui.setComponents(registry)`；C 使用 `oneui_view_mount_set_components_v1(mount, registry)`。设置后可销毁注册表句柄，视图保留自己的不可变快照。子挂载继承快照。完整可编译例子见 [status_app](../examples/plugins/status_app/CMakeLists.txt)。

## 契约文件

每个工厂提交一份 `.onecomponents` 文本，编译器通过 `--components FILE` 读取同一份文件。组件名使用 `Namespace_Component`，自定义属性 ID 从 1000 开始，事件 ID 从 1 开始。

```text
oneui-components 1
component Acme_Meter 1 leaf
property 1000 label utf8 3
property 1001 checked bool 6
model 1001
event 1 activate 0
end
```

属性类型支持 `bool`、`i64`、`f64`、`utf8`。标志按位组合：1 为静态赋值，2 为单向绑定，4 为双向绑定。双向属性同时设置 2 和 4，并用 `model` 指定一个属性。`leaf` 不接收子节点；也可声明 `any`、`one`、`optional-one`、`two`。事件可附带至多 16 个命名标量参数，例如 `event 2 seek 1 position f64`。

布局与语义共用属性为 `class`、`grow`、`basis`、`min`、`max`、`shrink`、`visible`、`disabled`、`name`、`description`。编译器把完整契约写入页面；运行时在创建任何控件和调用业务对象前检查注册契约。变更已有 ID、类型、事件参数或子节点规则时增加组件契约版本，并重新编译页面；包自身另外遵循 SemVer。

## 实现工厂

公开入口是 `oneui/oneui_component_c_api.h` 的 `OneUiComponentFactoryV1`。`create` 返回新建、未挂载的控件句柄和独立实例上下文；`set` 接收经过校验的属性；`disconnect` 停止订阅；`release_instance` 释放实例上下文。可选 `style_changed` 提醒控件重新查询主题。工厂数据的 retain/release 必须成对。

C++ 控件继承公开 `Widget`，使用公开 `Canvas` 绘制，通过 `createNativeWidgetHandle` 转移句柄。实例上下文使用弱引用指向 Widget，避免控件与实例循环持有。C 或其他语言也可在工厂内使用公开 C API 创建控件并返回其句柄。不要使用框架私有头文件。

控件通过 `host.emit` 发送声明的事件，通过 `host.change` 写回双向属性，通过 `host.style` 查询前景、背景、边框、字体大小与圆角。颜色为 `0xRRGGBBAA`。类选择器和组件类型选择器可用于布局与这些样式；控件自行消费绘制参数。实现键盘、焦点、指针和 `accessibilityInfo()`，使自定义控件保持原生输入与无障碍行为。

所有注册、工厂和 host 回调使用 UI 线程。属性更新和创建期间不能同步发送可变事件；返回 `REENTRANT` 时不要重试。后台结果先交给平台 UI 调度。UTF-8 属性和事件参数在调用期间借用，需要保留时复制。

## 关闭、失败与重载

`create` 在获取资源前写入实例输出；每次尝试创建都会配对调用 `release_instance`，包括失败和空实例。失败时控件可能已销毁，清理函数须容忍弱引用失效。`disconnect` 保证一次，关闭事件门后再释放 ViewModel。外部持有的控件可以继续存活，此时 host 的事件与写回返回 `CLOSED`；host 本身只在 `release_instance` 前有效。

页面重载会重新创建外部组件，并保留业务 ViewModel。工厂创建失败时保留旧页面。需要跨重载保存的状态放在 ViewModel 或独立服务中。首版插件代码随应用驻留，应用退出前不卸载原生库。

## 从示例创建自己的包

复制 [plugins/status](../plugins/status/CMakeLists.txt) 和消费示例到独立目录，修改包 ID、CMake 导出、组件命名空间和版本。`oneui_component_contract` 把契约嵌入工厂，避免手工维护第二份字符串。安装规则把同一文件交付给页面编译器。`oneui-plugin.json` 的 `kind` 为 `component`，`components` 列出组件名及契约版本。通过[本地或 Git 源码依赖](plugin-packages.md)接入，随后为实际支持的目标发布预编译包。

Rust 侧可通过 `oneui-sys` 中的 `OneUiComponentFactoryV1` 与注册函数接入；回调使用 `extern "C"`，捕获 panic，不让 unwind 穿过 C ABI。Python/C#/Go 等通过其 FFI 调用同一 C ABI，句柄与回调必须保持到对应释放点。
