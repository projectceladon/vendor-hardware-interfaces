
#!/bin/sh

adb connect 10.66.253.117

adb root

adb connect 10.66.253.117

adb remount 

adb push ~/Android_U/out/target/product/caas/vendor/lib/android.vendor.hardware.camera.provider@2.4* /vendor/lib/

adb push ~/Android_U/out/target/product/caas/vendor/bin/hw/android.* /vendor/bin/hw/

adb push ~/Android_U/out/target/product/caas/vendor/lib/camera.vendor.device@* /vendor/lib/
adb push ~/Android_U/out/target/product/caas/vendor/lib/hw/* /vendor/lib/hw/

adb reboot
