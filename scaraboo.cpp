/* dro project
 */

#include "wtf.h" //breakpoint for unhandled conditions

#include "gpio.h"
#include "nvic.h"
#include "clocks.h"

#include "systick.h"
using namespace SystemTimer;

#include "polledtimer.h"

#include "core_cmInstr.h"  //wfe OR wfi
#include "cruntime.h"
ClockStarter startup InitStep(InitHardware/2) (true,0,1000);//external wasn't working properly, need a test to check before switching to it.


#include "bluepill.h" //declares a board.



#include "fifo.h"

uint8_t outbuf[33];
Fifo outgoing(sizeof(outbuf),outbuf);

uint8_t inbuf[33];
Fifo incoming(sizeof(inbuf),inbuf);


#include "uart.h"
Uart theUart(1,1);//B6&7 please.

/** called by isr on an input event.
 * negative values of @param are notifications of line errors, -1 for interrupts disabled */
bool uartReceiver(int indata){
  if(incoming.insert(indata)){
    return true;
  } else {
    wtf(24);
    return false;
  }
}

/** called by isr when transmission becomes possible.
 *  @return either an 8 bit unsigned character, or -1 to disable transmission events*/
int uartSender(){
  int outdata=outgoing.remove();
  return outdata;//negative for fifo empty
}

void prepUart(){
  theUart.takePins(true,true);
  theUart.setFraming("N81");
  theUart.setBaud(115200);

  theUart.reception(true);
  theUart.irq(true);
}

#include "minimath.h"



int main(void) {
  prepUart();
  CyclicTimer slowToggle(ticksForSeconds(3)); //since action is polled might as well wait until main to declare the object.
  //soft stuff
  int events=0;
  //  Exti::enablePin(otherPin);

  prime.enable();
  pushButton.enable();

  while (1) {
    stackFault();//useless here, present to test compilation.
    //re-enabling in the loop to preclude some handler shutting them down. That is unacceptable, although individual ones certainly can be masked.
    IrqEnable=1;//master enable
    MNE(WFE);//wait for interrupt or event (an event is an interruptive thing without a vector, you wakeup here and go looking for them)
    ++events;
    
    if(slowToggle.hasFired()){
      board.toggleLed();
      if(outgoing.insert('A'+(events&15))){
//        theUart.beTransmitting();
      }
    }
  }
  return 0;
}
