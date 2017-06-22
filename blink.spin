CON
   _clkmode = xtal1 + pll16x
   _xinfreq = 5_000_000

  LED1 = 26
  LED2 = 27
  LED3 = 16
  LED4 = 17

#ifdef SLOW
  DIVISOR = 2
#else
  DIVISOR = 8
#endif

PUB main : mask
  mask := |< LED1 | |< LED2 | |< LED3 | |< LED4
  OUTA := |< LED1 | |< LED3
  DIRA := mask
  repeat
    OUTA ^= mask
    waitcnt(CNT + CLKFREQ / DIVISOR)
