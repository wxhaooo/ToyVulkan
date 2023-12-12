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

## 2023-11-17

* 简单重构一下代码，增加GrphicSetting，增加IMGUI的RenderPass （打算先将结果渲染到IMGUI中）

## 2023-11-20 -> 2023-11-22

* 一直在想办法将渲染结果到IMGUI中，debug了好几天，终于搞完了，
> 后面规划是：
> 1. 整理一下怎么将渲染结果显示在IMGUI中
> 2. 搞docking IMGUI
> 3. IMGUI响应键鼠事件
> 4. 做deferred rendering

## 2023-11-23

* 修复放大缩小时offscreenPass资源创建的bug
* 增加dockingGUI

## 2023-11-25

* IMGUI响应鼠标事件
> IMGUI不能得到当前选择的窗口，所以是通过鼠标悬停的窗口判断的，可能有点区别

## 2023-11-26 -> 2023-11-28

* 将descriptorSet的创建和更新都塞到资源里面，pipelineLayout塞到pipeline的创建流程里

## 2023-11-29

修复相机移动的bug，尝试将一些复杂的gltf资源导入。
tinygltf本来就是支持嵌入的jpg和png等格式的，不支持的只有ktx等压缩格式，所以不需要额外做什么，探索这个花了点时间。

## 2023-12-05

增加更多的GLTF支持

## 2023-12-06 -> 2023-12-11

重构一部分代码，增加VulkanRenderPass和VulkanFrameBuffer
实现mrtRenderPass，将Gbuffer的结果显示到imgui

## 2023-12-12

待做备忘：

* 渲染深度缓冲到GBuffer（以grayScale的方式）
* shader的自动编译
* 简单延迟光照
* PBR实现
* IBL实现
* 阴影的实现
* VulkanModel变为VulkanScene，scene管理多个模型、相机以及光照
* 其他需要重构的地方

存在的bug:

* 缩放窗口时也会导致glfw响应鼠标事件，造成相机视角问题