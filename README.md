ProtoCentral AF4490-based Pulse-oximeter shield for Arduino
================================
[![Oximeter](https://i2.wp.com/protocentral.com/wp-content/uploads/2020/10/4953.jpg?fit=629%2C600&ssl=1)  
*AFE4490 Pulse Oximeter Shield Kit for Arduino ](https://protocentral.com/product/protocentral-afe4490-pulse-oximeter-shield-for-arduino-v2/)

[![Oximeter](https://i1.wp.com/protocentral.com/wp-content/uploads/2020/10/4949.jpg?fit=689%2C628&ssl=1)  
*Don't have it yet? Buy one here: protocentral-afe4490-pulse-oximeter-breakout-board-kit* ](https://protocentral.com/product/protocentral-afe4490-pulse-oximeter-breakout-board-kit/)

This is the Arduino library for the ProtoCentral afe4490  breakout/shield boards.

Connecting the shield to your Arduino
-------------------------------------
 Connect the ECG/Respiration shield to the Arduino by stacking it on top of your Arduino. This shield uses the SPI interface  to communicate with the Arduino. Since this includes the ICSP header, which is used on newer Arduinos for SPI communication,  this shield is also compatible newer Arduino boards such as the Arduino Yun and Due.


Wiring the Breakout to your Arduino
------------------------------------
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

**NEW: This is now compatible with the [ProtoCentral OpenView](https://github.com/Protocentral/protocentral_openview) unified data visualization software.**



Using Processing - Data Visualization Software
---------------------------------------------
 Processing is a data visualization software, in existence since 2001, used by artists and scientists alike. It’s an open source coding framework based on Java. If you are familiar with the Arduino environment, the Processing IDE is similar and you won’t have much of a learning curve to climb!

 The following are the steps to run the code:

### 1. Download and Install Processing

 Download the processing ide latest version from the link

* [Download Processing for your OS] (http://download.processing.org/)

Once downloaded, unzip the archive and install the app as per your OS.

### 2. Download the Processing code for Pulse Oximeter visualization

 a. Download the necessary files & directories or clone to your desktop from GitHub.

 b. Unzipping the archive should make a folder by name AFE4490 Oximeter Shield that contains the visualization code.

 c. Locate the Processing sketchbook directory on your computer. This should have been created automatically when you installed processing. Depending on your operating system, the path will be as follows:

* On Windows: c:/My Documents/Processing/
* On MAC: /Users/your_user_name/Documents/Processing/
* On Linux: /Home/your_user_name/sketchbook/

###  Running the Arduino Sketch

Install the protoCentral afe4490 library from arduino library manager.
If you have correctly installed the libraries, the example sketeches should now be available from within Arduino.

Upload the code to your arduino and open [ProtoCentral-openview](https://github.com/Protocentral/protocentral_openview) GUI/Arduino plotter to view the output.

## For the main documentation site, GUI and more resources, please check out our main [GitHub Repo](https://github.com/Protocentral/AFE4490_Oximeter)



License Information
===================

This product is open source! Both, our hardware and software are open source and licensed under the following licenses:

Hardware
---------

**All hardware is released under [Creative Commons Share-alike 4.0 International](http://creativecommons.org/licenses/by-sa/4.0/).**
![CC-BY-SA-4.0](https://i.creativecommons.org/l/by-sa/4.0/88x31.png)

You are free to:

* Share — copy and redistribute the material in any medium or format
* Adapt — remix, transform, and build upon the material for any purpose, even commercially.
The licensor cannot revoke these freedoms as long as you follow the license terms.

Under the following terms:

* Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
* ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

Software
--------

**All software is released under the MIT License(http://opensource.org/licenses/MIT).**

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


Please check [*LICENSE.md*](LICENSE.md) for detailed license descriptions.
