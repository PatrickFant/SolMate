/*
 * rtc.c
 *
 *  Created on: Apr 20, 2016
 *      Author: User
 */

#include "rtc.h"

void rtc_initialize()
{
  // Set up the timer intervals, etc... it will count by seconds
  // (32768/256)/128 = 1
  RTCCTL01 |= RTCSSEL__RT1PS; // source the 32bit counter from RT1PS
  RTCPS0CTL |= RT0PSDIV_7; // divide ACLK by /256
  RTCPS1CTL |= RT1PSDIV_6 | RT1SSEL_2; // use RT0PS for clock input, divide by 128

  // Reset the time to a day in (86400 seconds -> 0x15180)
  RTCTIM0 = 0x5180; // low 16 bits
  RTCTIM1 = 0x1;    // high 16 bits

  // Start all the timers
  RTCPS0CTL &= ~RT0PSHOLD;
  RTCPS1CTL &= ~RT1PSHOLD;
  RTCCTL01 &= ~RTCHOLD;
}

/*
#pragma vector=RTC_VECTOR
__interrupt void rtc_interrupt()
{
  switch(RTCIV)
  {
  case RTCIV_RT1PSIFG:
    LED_PORT_OUT ^= LED_MSP_2;
  default:
    break;
  }
}*/
