/* JS2300 UI-mode smoke script used by the Linux/QEMU integration test. */
JS2300.video.clear(0x0000);
JS2300.video.text(12, 24, "JS2300 UI OK", 0xffff);
JS2300.video.text(12, 48, "mode=" + JS2300.mode(), 0x07e0);
JS2300.video.present();
JS2300.log("ui smoke complete mode=" + JS2300.mode());
