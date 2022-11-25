//////////////////////////////////////////////////////////////////////////////////////////
//
//    Arduino Library for the AFE44XX breakout board/shield from ProtoCentral Electronics
//
//    Author: Joice Tm
//    Copyright (c) 2020 ProtoCentral
//
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//    NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
/////////////////////////////////////////////////////////////////////////////////////////

/******   spo2 algorithm is based on maxim integrated algorithm relsead under MAXREFDES117#  *******/ 

/** \file algorithm.cpp ******************************************************
*
* Project: MAXREFDES117#
* Filename: algorithm.cpp
* Description: This module calculates the heart rate/SpO2 level
*
*
* --------------------------------------------------------------------
*
* This code follows the following naming conventions:
*
* char              ch_pmod_value
* char (array)      s_pmod_s_string[16]
* float             f_pmod_value
* int32_t           n_pmod_value
* int32_t (array)   an_pmod_value[16]
* int16_t           w_pmod_value
* int16_t (array)   aw_pmod_value[16]
* uint16_t          uw_pmod_value
* uint16_t (array)  auw_pmod_value[16]
* uint8_t           uch_pmod_value
* uint8_t (array)   auch_pmod_buffer[16]
* uint32_t          un_pmod_value
* int32_t *         pn_pmod_value
*
* ------------------------------------------------------------------------- */
/*******************************************************************************
* Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/


#include "protocentral_afe44xx.h"

#define AFE44XX_SPI_SPEED 2000000
SPISettings SPI_SETTINGS(AFE44XX_SPI_SPEED, MSBFIRST, SPI_MODE0); 

static  int32_t an_x[ BUFFER_SIZE]; 
static  int32_t an_y[ BUFFER_SIZE]; 
volatile int drdy_trigger = false;
volatile boolean afe44xx_data_ready = false;
unsigned long IRtemp,REDtemp;
int dec=0;
volatile int8_t n_buffer_count; //data length
int32_t n_spo2;  //SPO2 value
int8_t ch_spo2_valid;  //indicator to show if the SPO2 calculation is valid
int32_t n_heart_rate; //heart rate value
int8_t  ch_hr_valid;  //indicator to show if the heart rate calculation is valid
uint16_t aun_ir_buffer[100]; //infrared LED sensor data
uint16_t aun_red_buffer[100];  //red LED sensor data

const uint8_t uch_spo2_table[184]={ 95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99, 
              99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 
              100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97, 
              97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91, 
              90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81, 
              80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67, 
              66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 
              49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29, 
              28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5, 
              3, 2, 1 } ;

AFE44XX::AFE44XX(int cs_pin, int pwdn_pin, int drdy_pin, int intr_num)
{
    _cs_pin=cs_pin;
    _drdy_pin=drdy_pin;

    pinMode(_cs_pin, OUTPUT);
    digitalWrite(_cs_pin,HIGH);

    pinMode (pwdn_pin,OUTPUT);
    pinMode (drdy_pin,INPUT);// data ready

    intrrpt_num = intr_num;
    attachInterrupt(digitalPinToInterrupt(_drdy_pin), afe44xx_drdy_event, RISING );
    //attachInterrupt(intrrpt_num, afe44xx_drdy_event, RISING );

    digitalWrite(pwdn_pin, LOW);
    delay(500);
    digitalWrite(pwdn_pin, HIGH);
    delay(500);
}

void afe44xx_drdy_event()
{
  drdy_trigger = true;
}

boolean AFE44XX :: getDataIfAvailable (afe44xx_output_values *sensed_values)
{
  if (drdy_trigger )
  {
    //detachInterrupt(intrrpt_num);
    detachInterrupt(digitalPinToInterrupt(_drdy_pin));
    afe44xxWrite(CONTROL0, 0x000001);
    IRtemp = afe44xxRead(LED1VAL);
    afe44xxWrite(CONTROL0, 0x000001);
    REDtemp = afe44xxRead(LED2VAL);
    afe44xx_data_ready = true;
  }

  if (afe44xx_data_ready == true)
  {
    IRtemp = (unsigned long) (IRtemp << 10);
    sensed_values->ir = (signed long) (IRtemp);
    sensed_values->ir = (signed long) ((sensed_values->ir) >> 10);
    REDtemp = (unsigned long) (REDtemp << 10);
    sensed_values->red = (signed long) (REDtemp);
    sensed_values->red = (signed long) ((sensed_values->red) >> 10);

    if (dec == 20)
    {
      aun_ir_buffer[n_buffer_count] = (uint16_t) ((sensed_values->ir) >> 4);
      aun_red_buffer[n_buffer_count] = (uint16_t) ((sensed_values->red) >> 4);
      n_buffer_count++;
      dec = 0;
    }

    dec++;

    if (n_buffer_count > 99)
    {
      estimate_spo2(aun_ir_buffer, 100, aun_red_buffer, &n_spo2, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
      sensed_values->spo2 = n_spo2;
      sensed_values->heart_rate = n_heart_rate;
      n_buffer_count = 0;

      return true;
    }
    
    sensed_values->calculated_value = true;
    afe44xx_data_ready = false;
    drdy_trigger = false;
    attachInterrupt(digitalPinToInterrupt(_drdy_pin), afe44xx_drdy_event, RISING ); 

  }else{

    return false;
  }
}


void AFE44XX::afe44xx_init()
{
  /*pinMode (cs_pin,OUTPUT);//Slave Select
  pinMode (pwdn,OUTPUT);
  pinMode (drdy,INPUT);// data ready

  intrrpt_num = interrupt_num;
  attachInterrupt(intrrpt_num, afe44xx_drdy_event, RISING );

  digitalWrite(pwdn, LOW);
  delay(500);
  digitalWrite(pwdn, HIGH);
  delay(500);
  */

  afe44xxWrite(CONTROL0, 0x000000);
  afe44xxWrite(CONTROL0, 0x000008);
  afe44xxWrite(TIAGAIN, 0x000000); // CF = 5pF, RF = 500kR
  afe44xxWrite(TIA_AMB_GAIN, 0x000001);
  afe44xxWrite(LEDCNTRL, 0x001414);
  afe44xxWrite(CONTROL2, 0x000000); // LED_RANGE=100mA, LED=50mA
  afe44xxWrite(CONTROL1, 0x010707); // Timers ON, average 3 samples
  afe44xxWrite(PRPCOUNT, 0X001F3F);
  afe44xxWrite(LED2STC, 0X001770);
  afe44xxWrite(LED2ENDC, 0X001F3E);
  afe44xxWrite(LED2LEDSTC, 0X001770);
  afe44xxWrite(LED2LEDENDC, 0X001F3F);
  afe44xxWrite(ALED2STC, 0X000000);
  afe44xxWrite(ALED2ENDC, 0X0007CE);
  afe44xxWrite(LED2CONVST, 0X000002);
  afe44xxWrite(LED2CONVEND, 0X0007CF);
  afe44xxWrite(ALED2CONVST, 0X0007D2);
  afe44xxWrite(ALED2CONVEND, 0X000F9F);
  afe44xxWrite(LED1STC, 0X0007D0);
  afe44xxWrite(LED1ENDC, 0X000F9E);
  afe44xxWrite(LED1LEDSTC, 0X0007D0);
  afe44xxWrite(LED1LEDENDC, 0X000F9F);
  afe44xxWrite(ALED1STC, 0X000FA0);
  afe44xxWrite(ALED1ENDC, 0X00176E);
  afe44xxWrite(LED1CONVST, 0X000FA2);
  afe44xxWrite(LED1CONVEND, 0X00176F);
  afe44xxWrite(ALED1CONVST, 0X001772);
  afe44xxWrite(ALED1CONVEND, 0X001F3F);
  afe44xxWrite(ADCRSTCNT0, 0X000000);
  afe44xxWrite(ADCRSTENDCT0, 0X000000);
  afe44xxWrite(ADCRSTCNT1, 0X0007D0);
  afe44xxWrite(ADCRSTENDCT1, 0X0007D0);
  afe44xxWrite(ADCRSTCNT2, 0X000FA0);
  afe44xxWrite(ADCRSTENDCT2, 0X000FA0);
  afe44xxWrite(ADCRSTCNT3, 0X001770);
  afe44xxWrite(ADCRSTENDCT3, 0X001770);

  delay(1000);
}

void AFE44XX :: afe44xxWrite (uint8_t address, uint32_t data)
{
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_cs_pin, LOW);

  SPI.transfer (address); // send address to device
  SPI.transfer ((data >> 16) & 0xFF); // write top 8 bits
  SPI.transfer ((data >> 8) & 0xFF); // write middle 8 bits
  SPI.transfer (data & 0xFF); // write bottom 8 bits

  digitalWrite (_cs_pin, HIGH); // disable device
  SPI.endTransaction();

}

unsigned long AFE44XX :: afe44xxRead (uint8_t address)
{
  unsigned long data = 0;
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_cs_pin, LOW);

  SPI.transfer (address); // send address to device
  
  data |= ((unsigned long)SPI.transfer (0) << 16); // read top 8 bits data
  data |= ((unsigned long)SPI.transfer (0) << 8); // read middle 8 bits  data
  data |= SPI.transfer (0); // read bottom 8 bits data
  digitalWrite (_cs_pin, HIGH); // disable device
  SPI.endTransaction();
  return data; // return with 24 bits of read data
}

void AFE44XX :: estimate_spo2(uint16_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint16_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{
  uint32_t un_ir_mean,un_only_once ;
  int32_t k, n_i_ratio_count;
  int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
  int32_t n_th1, n_npks, n_c_min;   
  int32_t an_ir_valley_locs[15] ;
  int32_t n_peak_interval_sum;
  
  int32_t n_y_ac, n_x_ac;
  int32_t n_spo2_calc; 
  int32_t n_y_dc_max, n_x_dc_max; 
  int32_t n_y_dc_max_idx, n_x_dc_max_idx; 
  int32_t an_ratio[5], n_ratio_average; 
  int32_t n_nume, n_denom ;

  // calculates DC mean and subtract DC from ir
  un_ir_mean =0; 
  for (k=0 ; k<n_ir_buffer_length ; k++ ) un_ir_mean += pun_ir_buffer[k] ;
  un_ir_mean =un_ir_mean/n_ir_buffer_length ;
    
  // remove DC and invert signal so that we can use peak detector as valley detector
  for (k=0 ; k<n_ir_buffer_length ; k++ )  
    an_x[k] = -1*(pun_ir_buffer[k] - un_ir_mean) ; 
    
  // 4 pt Moving Average
  for(k=0; k< BUFFER_SIZE-MA4_SIZE; k++){
    an_x[k]=( an_x[k]+an_x[k+1]+ an_x[k+2]+ an_x[k+3])/(int)4;        
  }
  // calculate threshold  
  n_th1=0; 
  for ( k=0 ; k<BUFFER_SIZE ;k++){
    n_th1 +=  an_x[k];
  }
  n_th1=  n_th1/ ( BUFFER_SIZE);
  if( n_th1<30) n_th1=30; // min allowed
  if( n_th1>60) n_th1=60; // max allowed

  for ( k=0 ; k<15;k++) an_ir_valley_locs[k]=0;
  // since we flipped signal, we use peak detector as valley detector
  find_peak( an_ir_valley_locs, &n_npks, an_x, BUFFER_SIZE, n_th1, 4, 15 );//peak_height, peak_distance, max_num_peaks 
  n_peak_interval_sum =0;
  if (n_npks>=2){
    for (k=1; k<n_npks; k++) n_peak_interval_sum += (an_ir_valley_locs[k] -an_ir_valley_locs[k -1] ) ;
    n_peak_interval_sum =n_peak_interval_sum/(n_npks-1);
    *pn_heart_rate =(int32_t)( (FS*60)/ n_peak_interval_sum );
    *pch_hr_valid  = 1;
  }
  else  { 
    *pn_heart_rate = -999; // unable to calculate because # of peaks are too small
    *pch_hr_valid  = 0;
  }

  //  load raw value again for SPO2 calculation : RED(=y) and IR(=X)
  for (k=0 ; k<n_ir_buffer_length ; k++ )  {
      an_x[k] =  pun_ir_buffer[k] ; 
      an_y[k] =  pun_red_buffer[k] ; 
  }

  // find precise min near an_ir_valley_locs
  n_exact_ir_valley_locs_count =n_npks; 
  
  //using exact_ir_valley_locs , find ir-red DC andir-red AC for SPO2 calibration an_ratio
  //finding AC/DC maximum of raw

  n_ratio_average =0; 
  n_i_ratio_count = 0; 
  for(k=0; k< 5; k++) an_ratio[k]=0;
  for (k=0; k< n_exact_ir_valley_locs_count; k++){
    if (an_ir_valley_locs[k] > BUFFER_SIZE ){
      *pn_spo2 =  -999 ; // do not use SPO2 since valley loc is out of range
      *pch_spo2_valid  = 0; 
      return;
    }
  }
  // find max between two valley locations 
  // and use an_ratio betwen AC compoent of Ir & Red and DC compoent of Ir & Red for SPO2 
  for (k=0; k< n_exact_ir_valley_locs_count-1; k++){
    n_y_dc_max= -16777216 ; 
    n_x_dc_max= -16777216; 
    if (an_ir_valley_locs[k+1]-an_ir_valley_locs[k] >3){
        for (i=an_ir_valley_locs[k]; i< an_ir_valley_locs[k+1]; i++){
          if (an_x[i]> n_x_dc_max) {n_x_dc_max =an_x[i]; n_x_dc_max_idx=i;}
          if (an_y[i]> n_y_dc_max) {n_y_dc_max =an_y[i]; n_y_dc_max_idx=i;}
      }
      n_y_ac= (an_y[an_ir_valley_locs[k+1]] - an_y[an_ir_valley_locs[k] ] )*(n_y_dc_max_idx -an_ir_valley_locs[k]); //red
      n_y_ac=  an_y[an_ir_valley_locs[k]] + n_y_ac/ (an_ir_valley_locs[k+1] - an_ir_valley_locs[k])  ; 
      n_y_ac=  an_y[n_y_dc_max_idx] - n_y_ac;    // subracting linear DC compoenents from raw 
      n_x_ac= (an_x[an_ir_valley_locs[k+1]] - an_x[an_ir_valley_locs[k] ] )*(n_x_dc_max_idx -an_ir_valley_locs[k]); // ir
      n_x_ac=  an_x[an_ir_valley_locs[k]] + n_x_ac/ (an_ir_valley_locs[k+1] - an_ir_valley_locs[k]); 
      n_x_ac=  an_x[n_y_dc_max_idx] - n_x_ac;      // subracting linear DC compoenents from raw 
      n_nume=( n_y_ac *n_x_dc_max)>>7 ; //prepare X100 to preserve floating value
      n_denom= ( n_x_ac *n_y_dc_max)>>7;
      if (n_denom>0  && n_i_ratio_count <5 &&  n_nume != 0)
      {   
        an_ratio[n_i_ratio_count]= (n_nume*100)/n_denom ; //formular is ( n_y_ac *n_x_dc_max) / ( n_x_ac *n_y_dc_max) ;
        n_i_ratio_count++;
      }
    }
  }
  // choose median value since PPG signal may varies from beat to beat
  sort_ascend(an_ratio, n_i_ratio_count);
  n_middle_idx= n_i_ratio_count/2;

  if (n_middle_idx >1)
    n_ratio_average =( an_ratio[n_middle_idx-1] +an_ratio[n_middle_idx])/2; // use median
  else
    n_ratio_average = an_ratio[n_middle_idx ];

  if( n_ratio_average>2 && n_ratio_average <184){
    n_spo2_calc= uch_spo2_table[n_ratio_average] ;
    *pn_spo2 = n_spo2_calc ;
    *pch_spo2_valid  = 1;//  float_SPO2 =  -45.060*n_ratio_average* n_ratio_average/10000 + 30.354 *n_ratio_average/100 + 94.845 ;  // for comparison with table
  }
  else{
    *pn_spo2 =  -999 ; // do not use SPO2 since signal an_ratio is out of range
    *pch_spo2_valid  = 0; 
  }
}


void AFE44XX :: find_peak( int32_t *pn_locs, int32_t *n_npks,  int32_t  *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num )
/**
  \brief        Find peaks
  \par          Details
                Find at most MAX_NUM peaks above MIN_HEIGHT separated by at least MIN_DISTANCE

  \retval       None
*/
{
  find_peak_above( pn_locs, n_npks, pn_x, n_size, n_min_height );
  remove_close_peaks( pn_locs, n_npks, pn_x, n_min_distance );
  *n_npks = min( *n_npks, n_max_num );
}

void AFE44XX :: find_peak_above( int32_t *pn_locs, int32_t *n_npks,  int32_t  *pn_x, int32_t n_size, int32_t n_min_height )
/**
  \brief        Find peaks above n_min_height
  \par          Details
                Find all peaks above MIN_HEIGHT

  \retval       None
*/
{
  int32_t i = 1, n_width;
  *n_npks = 0;

  while (i < n_size - 1) {
    if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i - 1]) {   // find left edge of potential peaks
      n_width = 1;
      while (i + n_width < n_size && pn_x[i] == pn_x[i + n_width]) // find flat peaks
        n_width++;
      if (pn_x[i] > pn_x[i + n_width] && (*n_npks) < 15 ) {   // find right edge of peaks
        pn_locs[(*n_npks)++] = i;
        // for flat peaks, peak location is left edge
        i += n_width + 1;
      }
      else
        i += n_width;
    }
    else
      i++;
    //  Serial.println("beat");
  }
}

void AFE44XX :: remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance)
/**
  \brief        Remove peaks
  \par          Details
                Remove peaks separated by less than MIN_DISTANCE

  \retval       None
*/
{

  int32_t i, j, n_old_npks, n_dist;

  /* Order peaks from large to small */
  sort_indices_descend( pn_x, pn_locs, *pn_npks );

  for ( i = -1; i < *pn_npks; i++ ) {
    n_old_npks = *pn_npks;
    *pn_npks = i + 1;
    for ( j = i + 1; j < n_old_npks; j++ ) {
      n_dist =  pn_locs[j] - ( i == -1 ? -1 : pn_locs[i] ); // lag-zero peak of autocorr is at index -1
      if ( n_dist > n_min_distance || n_dist < -n_min_distance )
        pn_locs[(*pn_npks)++] = pn_locs[j];
    }
  }

  // Resort indices int32_to ascending order
  sort_ascend( pn_locs, *pn_npks );
}

void AFE44XX :: sort_ascend(int32_t  *pn_x, int32_t n_size)
/**
  \brief        Sort array
  \par          Details
                Sort array in ascending order (insertion sort algorithm)

  \retval       None
*/
{
  int32_t i, j, n_temp;
  for (i = 1; i < n_size; i++) {
    n_temp = pn_x[i];
    for (j = i; j > 0 && n_temp < pn_x[j - 1]; j--)
      pn_x[j] = pn_x[j - 1];
    pn_x[j] = n_temp;
  }
}

void AFE44XX :: sort_indices_descend(  int32_t  *pn_x, int32_t *pn_indx, int32_t n_size)
/**
  \brief        Sort indices
  \par          Details
                Sort indices according to descending order (insertion sort algorithm)

  \retval       None
*/
{
  int32_t i, j, n_temp;
  for (i = 1; i < n_size; i++) {
    n_temp = pn_indx[i];
    for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j - 1]]; j--)
      pn_indx[j] = pn_indx[j - 1];
    pn_indx[j] = n_temp;
  }
}
