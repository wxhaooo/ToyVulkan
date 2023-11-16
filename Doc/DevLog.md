## 2023-11-13 以前

vulkanExample中的东西重新整理并重写了下gltfLoading这个case。

## 2023-11-13

* 修复Window调整窗口时的重建swapChain引起的bugs，修复方法是在重建swapchain的时候将currentFrame设置为0（`VulkanApplicationBase::ReCreateVulkanResource`），但是在VulanTutorial这个例子里不这么做validateLayer不会不错，很神奇，先记录一下。

## 2023-11-14

* 增加camera的setLookUp的功能
* 将Imgui替换为docking版本

> 接下来打算：
> 1. 将以前尝试过的Imgui合并进application
> 2. 处理键鼠事件
> 3. 对代码进行整理
> 4. 做deferred rendering这个例子
>
## 2023-11-15

* 迁移和适配以前写的ImGUI的代码

## 2023-11-16

* 修复适配后ImGUI的一些bug，先做了一个简单的测试，目前ImGUI显示在模型下面，需要修复一下

