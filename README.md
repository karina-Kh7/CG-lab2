# Лабораторная работа №1 по компьютерной графике

## Вариант

Вариант 11 — правильный икосаэдр.

## Изменения стартового проекта

В качестве основы использован стартовый Vulkan-проект, предложенный в курсе.

Для запуска проекта на моей конфигурации потребовалось внести несколько изменений.

### CMake

В `CMakePresets.json` генератор был изменён с

```text
Visual Studio 17 2022
```

на

```text
Visual Studio 18 2026
```

так как проект собирался с использованием Visual Studio 2026.

### ImGui

Для используемой версии Dear ImGui 1.92.9 в основном цикле были добавлены вызовы:

```cpp
ImGui_ImplVulkan_NewFrame();
ImGui_ImplGlfw_NewFrame();
ImGui::NewFrame();
```

Также был добавлен вызов:

```cpp
ImGui::Render();
```

### Render

В `application.cpp` была добавлена запись команд в `VkCommandBuffer`, чтобы основной render pass корректно выполнялся.

Используются вызовы:

```cpp
vkResetCommandBuffer(...);
vkBeginCommandBuffer(...);
vkCmdBeginRenderPass(...);
```

После привязки pipeline, vertex/index buffers и descriptor set выполняется отрисовка:

```cpp
vkCmdDrawIndexed(...);
```

Затем render pass и command buffer завершаются:

```cpp
vkCmdEndRenderPass(...);
vkEndCommandBuffer(...);
```

### Descriptor pool ImGui

В `graphics_internal.cpp` была скорректирована настройка descriptor pool для ImGui, чтобы количество доступных descriptor sets соответствовало используемому swapchain.

## Сборка

Для работы требуется установленный Vulkan SDK.

Проект собирался с использованием Visual Studio 2026 и CMake.

Из корневой директории проекта выполнить:

"D:\vs2026\install\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --preset msvc-debug

Затем:

```cmd
"D:\vs2026\install\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-debug --config Debug --parallel
```

## Запуск

Из корневой директории проекта:

```cmd
build-debug\Debug\vulkan-starter-app.exe
```

Программу необходимо запускать из корневой директории проекта, так как shader-файлы загружаются из директории shaders.