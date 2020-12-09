# esp-epaper

This is library for driving a 2.13" e-paper display (GDEH0213B73) on a ESP32 using the esp-idf (master branch), all tested with this module: https://github.com/Xinyuan-LilyGO/LilyGo-T5-ink-series/tree/master/LilyGo_T5_V2.3

The code is mainly based on this repository here: https://github.com/fabiomarini/ESP32_ePaper_example. As the ePaper example didn't work for my display I've reworked it into something new (also inspired by https://github.com/ZinggJM/GxEPD). 

All this is still work in progress, but the basis works fine. It also needs some code refactoring to make it more clean and readable (ex: split the font functions into a separate draw file).





