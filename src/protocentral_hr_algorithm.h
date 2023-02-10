
//////////////////////////////////////////////////////////////////////////////////////////
//
//    This example is only for the ProtoCentral OpenOx Board (coming soon)
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   For information on how to use, visit https://github.com/Protocentral/protocentral-afe4490-arduino
//
/////////////////////////////////////////////////////////////////////////////////////////

/*
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/ 
 * ALL RIGHTS RESERVED  
 *
*/
#ifndef _HRM_H_
#define _HRM_H_

class hr_algo
{
  public:
	void initStatHRM (void);
  void statHRMAlgo (unsigned long ppgData);

  void updateWindow(unsigned long *peakWindow, unsigned long Y, unsigned char n);
  unsigned char chooseRate(unsigned char *rate);
  void updateHeartRate(unsigned char *rate, unsigned int freq, unsigned int last);
  unsigned long findMax(unsigned long *X);
  unsigned long findMin(unsigned long *X);

  unsigned char HeartRate=0;
};



#endif /*_HRM_H_*/
