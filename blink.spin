CON
'   _clkmode = RCFAST

   _clkmode = xtal1 + pll16x
   _xinfreq = 5_000_000

  LED1 = 26
  LED2 = 27

PUB main : mask
  mask := |< LED1 | |< LED2
  OUTA := |< LED1
  DIRA := mask
  repeat
    OUTA ^= mask
    waitcnt(CNT + CLKFREQ / 8)
