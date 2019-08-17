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
/** called by isr on an input event.
 * negative values of @param are notifications of line errors, -1 for interrupts disabled */
void uartReceiver(int indata){
  if(!incoming.insert(indata)){
    wtf(24);//comm sending failure.
  }
}


#include "uartuser.h"
UartUser theUart(uartReceiver,1,1);//B6&7 please.

//
///** called by isr when transmission becomes possible.
// *  @return either an 8 bit unsigned character, or -1 to disable transmission events*/
//int uartSender(){
//  int outdata=theUart.transmit(outgoing);
//  return outdata;//negative for fifo empty
//}


HandleInterrupt(37 ){//37 is uart1, unfortunately we need a string of decimal digits here, it is used by preprocessor to generate irq routine name.
  theUart.uisr();
}

void prepUart(){
  theUart.init(115200);//includes enabling interrupt
  theUart.reception(true);
}


/** extract of fancier classes */
class HBridge {
  const OutputPin plus;
  const OutputPin minus;
public:
  HBridge(Pin &&pplus, Pin &&pminus):plus(pplus),minus(pminus){}

  bool operator =(bool dir)const {
    plus=dir;
    minus=!dir;
    return dir;
  }

  void brake()const{
    plus=0;
    minus=0;
  }
  void off()const{
    plus=1;
    minus=1;
  }

};


/** not including timers at thi slevel as we are experimenting with different timers, and may separate pulse time fro step interval */
class SimpleStepper {
public:
  int location;
  int target;
  //later: unsigned phaseshift; //coils settings at position 0.
    const HBridge x;
    const HBridge y;
    SimpleStepper (const HBridge &x,const HBridge &y):x(x),y(y){    }


  /** accept a comparison result to choose a direction, if any, to step.
    @returns whether a change of drive was made.
  */

    bool operator() (int dir){
      if(dir==0){
        return false;
      }
      if(dir>0){
        ++location;      
      } else {
        --location;
      }
      bool gray2=(location&2)!=0;
        bool gray1=(location&1)!=0;
        y=gray2;
        x=gray2!=gray1;
        
      return true;
    }

    /** call this after a step has settled down */
    bool onStepComplete(){
      return this->operator()(target-location);
    }

    void off(){
    //todo: disable interrupts around this pair
      x.off();
      y.off();
    }

} motor(HBridge(Pin(PA,8),Pin(PA,9)),HBridge(Pin(PA,10),Pin(PA,11)));


#include "timer.h"

struct TimeInterval{
  double seconds;
  int ticks;
} steprate;

PeriodicInterrupter steptick(4);

void setupTicker(){
  steprate.seconds=1.0/200.0;
  unsigned tickpersecond=steptick.baseRate();
  steprate.ticks=tickpersecond*steprate.seconds;//ignore rounding for now.
  motor.location=0;
  motor.target=0;
  motor.off();
}


HandleInterrupt(30){//30:timer4
  bool another=motor.onStepComplete();
  if(another){//todo: arrange for this outside of isr. 
    steptick.restart(steprate.ticks);//#do not use float version here, we are in an isr!
  }
}

#include "minimath.h"


int main(void) {
  prepUart();
  CyclicTimer slowToggle(ticksForSeconds(3)); //since action is polled might as well wait until main to declare the object.
  int events=0;
  setupTicker();
  
  steptick.restart(steprate.ticks);//often will lose one step as first step just energizes motor
  while (1) {
    //re-enabling in the loop to preclude some handler shutting them down. That is unacceptable, although individual ones certainly can be masked.
    IrqEnable=1;//master enable
    MNE(WFE);//wait for interrupt or event (an event is an interruptive thing without a vector, you wakeup here and go looking for them)
    ++events;
    
    if(slowToggle.hasFired()){
      board.toggleLed();//testing feature
    }

    //todo: read uart and modify stepper state and speed.
    int key;
    while((key=incoming.attempt_remove())>0){
      //command interpreter goes here.
        //for now do case inverting echo
      if(outgoing.insert(key^0x20)){
        theUart.u.beTransmitting();
      }
    }
  }
  return 0;//never will get here but we dislike the compiler complaining :)
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//unexpected link to operator delete [] from cstr, but there is no new or delete in that module. We get these things each time we update compiler and libraries, in this case some library ain't obeying all the rules.
extern "C" void free(void *){
 //the lib includes free which then looks for some linker fields we don't allow.
  wtf(666);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
