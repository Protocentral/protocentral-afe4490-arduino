Protocentral AF4490/AFE4400 based PPG/SpO2/HR shield for Arduino
================================
[![Compile Examples](https://github.com/Protocentral/protocentral-afe4490-arduino/workflows/Compile%20Examples/badge.svg)](https://github.com/Protocentral/protocentral-afe4490-arduino/actions?workflow=Compile+Examples)

![Oximeter](https://i2.wp.com/protocentral.com/wp-content/uploads/2020/10/4953.jpg?fit=629%2C600&ssl=1)  

## Don't have shield? [Buy it here](https://protocentral.com/product/protocentral-afe4490-pulse-oximeter-shield-for-arduino-v2/)

![Oximeter](https://i1.wp.com/protocentral.com/wp-content/uploads/2020/10/4949.jpg?fit=689%2C628&ssl=1)  

## Don't have breakout board? [Buy it here](https://protocentral.com/product/protocentral-afe4490-pulse-oximeter-breakout-board-kit/)

## Hardware Setup

### Connecting the shield to your Arduino

Connect the ECG/Respiration shield to the Arduino by stacking it on top of your Arduino. This shield uses the SPI interface  to communicate with the Arduino. Since this includes the ICSP header, which is used on newer Arduinos for SPI communication,  this shield is also compatible newer Arduino boards such as the Arduino Yun and Due.


### Wiring the Breakout to your Arduino

If you have bought the breakout the connection with the Arduino board is as follows:

|AFE4490 pin label| Arduino Connection   |Pin Function                  |
|----------------- |:--------------------:|-----------------:           |
| GND              | Gnd                  |  Gnd                        |             
| DRDY             | D2                   |  Data ready(interrupt)      |
| MISO             | D12                  |  Slave out                  |
| SCK              | D13                  |  SPI clock                  |
| MOSI             | D11                  |  Slave in                   |
| CS0              | D7                   |  Slave select               |
| START            | D5                   |  Conversion start Pin       |
| PWDN             | D4                   |  Power Down/ Reset          |
| DIAG_END         | NC                   |  Diagnostic output          |
| LED_ALM          | NC                   |  Cable fault indicator      |
| PD_ALM           | NC                   |  PD sensor fault indicator  |
| VCC              | +5v                  |  Supply voltage             |

# Visualizing Output

![openview output](./assets/AFE4490_openview.gif)

## For further details, refer [the documentation on AFE4490 breakout Board](https://docs.protocentral.com/getting-started-with-AFE4490/)



License Information
===================

![License](license_mark.svg)

This product is open source! Both, our hardware and software are open source and licensed under the following licenses:

Hardware
---------

**All hardware is released under the [CERN-OHL-P v2](https://ohwr.org/cern_ohl_p_v2.txt)** license.

Copyright CERN 2020.

This source describes Open Hardware and is licensed under the CERN-OHL-P v2.

You may redistribute and modify this documentation and make products
using it under the terms of the CERN-OHL-P v2 (https:/cern.ch/cern-ohl).
This documentation is distributed WITHOUT ANY EXPRESS OR IMPLIED
WARRANTY, INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY
AND FITNESS FOR A PARTICULAR PURPOSE. Please see the CERN-OHL-P v2
for applicable conditions

Software
--------

**All software is released under the MIT License(http://opensource.org/licenses/MIT).**

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Documentation
-------------
**All documentation is released under [Creative Commons Share-alike 4.0 International](http://creativecommons.org/licenses/by-sa/4.0/).**
![CC-BY-SA-4.0](https://i.creativecommons.org/l/by-sa/4.0/88x31.png)

You are free to:

* Share — copy and redistribute the material in any medium or format
* Adapt — remix, transform, and build upon the material for any purpose, even commercially.
The licensor cannot revoke these freedoms as long as you follow the license terms.

Under the following terms:

* Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
* ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

Please check [*LICENSE.md*](LICENSE.md) for detailed license descriptions.
