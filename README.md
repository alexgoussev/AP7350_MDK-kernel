# AP7350_MDK
AP7350是深圳市雨滴科技有限公司开发的一款Android手机开发模块，基于MTK MT6735平台。
AP7350_MDK是配套的AP7350核心板的开发板。

首先，加载编译环境设置
----------------------
source ./build/envsetup.sh

其次，选择编译项目
------------------
运行lunch，输入12，选择full_ap7350_65u_l1-eng

编译preloader
------------
make preloader

编译little kernel
------------
make lk

编译内核
------------
make kernel

生成bootimage
-------------
mnake bootimage

生成recoveryimage
------------
make recoveryimage