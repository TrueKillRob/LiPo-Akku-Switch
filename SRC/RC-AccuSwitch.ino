#include<avr/cpufunc.h>
#include<avr/power.h>
#include<avr/sleep.h>
#include<avr/io.h>
#include<avr/interrupt.h>

#define _A_LED        5
#define _B_ACCU1_ON   0
#define _B_ACCU2_ON   1
#define _B_ACCU3_ON   2
#define _ADC_ACCU1    0
#define _ADC_ACCU2    1
#define _ADC_ACCU3    2
#define _ADC_CURR     3
#define _ADC_CurRef   7

#define LED_toggle    (PORTA ^= (1<<_A_LED))
#define LED_on        (PORTA |= (1<<_A_LED))
#define LED_off       (PORTA &= ~(1<<_A_LED))

// 0,1A bei Shunt = 0,01 Ohm --> 465
#define IDLE_Current  46

class average {
  private:
    uint8_t count = 0, max, done = 0;
    uint16_t values[6], avg = 0;
    bool chg = false;
  public:
    uint16_t getAVG()  {
      uint8_t i;
      uint16_t tmp = 0;
      if ( done == 0 )
        return 0;
      if ( avg == 0 ) {
        for( i=0; i<=done; i++ ) {
          tmp += values[i];
        }
        avg = tmp / done;
      }
      return avg;
    }
    average(void) {
      max = 5; 
    }
    
    void Add( uint16_t value ) {
      count %= max;
      count ++;
      values[count-1]=value;
      if ( done < count )
        done = count;
      avg = 0;
    }

    void setmax( uint8_t m ) { if ( m <= 6 ) max = m; }
    uint8_t getmax( void )   { return max; }
    uint8_t getcount( void ) { return count; }
};

void goToSleep(void);
uint16_t GetADC(uint8_t);

average AC1, AC2, AC3, CUR, REF;
uint8_t timer;
bool charge, Time1OVF;

void setup() {
  cli();
  charge = 0;
  Time1OVF = 0;
  ADCSRA = 0b10010111;   //ADC ein
  //         76543210
  DDRB   = 0b00000111;  
  PORTB  = 0b00000111;

  DDRA   = 0b00100000;
  
  MCUCR  &= ~((1<<SM0)|(1<<SM1)); //b01000000;

  GTCCR  = 0b00000000; 
  TIMSK1 |= (1<<TOIE1);
  timer = 0;
  sei(); //Enable interrupt
  TCNT1  = 0;
  TCCR1A = 0b00000000;
  TCCR1C = 0b00000000;
  TCCR1A = 0b00000010;    
  TCCR1B = 0b00000101;    //Set the prescale 8MHz / (1024 * 65.536) --> 8s Faktor
}

void loop() {
  uint16_t _A1, _A2, _A3, _CU, _CUR;
  uint8_t tmp = 0;

  //LED_toggle;
  //timer = 2;
  
  if ( timer == 1 ) {
    GetADC( _ADC_ACCU1 );
    AC1.Add ( GetADC( _ADC_ACCU1 ) ) ;
    AC2.Add ( GetADC( _ADC_ACCU2 ) ) ;
    AC3.Add ( GetADC( _ADC_ACCU3 ) ) ;
  }

  if ( timer == 2 ) {
//LED_toggle;
    GetADC( _ADC_CURR );
    CUR.Add ( GetADC( _ADC_CURR ) ) ;
    REF.Add ( GetADC( _ADC_CurRef ) ) ;
  }

/*  if( CUR.getAVG() >= IDLE_Current ) //REF.getAVG() )
    LED_on;
  else
    LED_off; */

  if ( timer == 3 ) {
    timer = 0;

    if ( AC1.getcount() > 3 && 
         AC2.getcount() > 3 && 
         AC3.getcount() > 3 && 
         CUR.getcount() > 3 ) {
  
       _A1 = AC1.getAVG();
       _A2 = AC2.getAVG();
       _A3 = AC3.getAVG();
       _CU = CUR.getAVG();
       _CUR = REF.getAVG();
       

      if ( _CU < ( _CUR + IDLE_Current ) && _CU > (_CUR - IDLE_Current ) ) {  // Idle
        timer = 100;
      }
       
      if ( _CU > _CUR ) {                   // Laden
        if ( charge == 0 ) {
          charge = 1;
          LED_on;
          tmp = 0b00000111;
        }
        if ( _A1 < _A2 && _A1 < _A3 ) {
          tmp &= ~(1<<_B_ACCU1_ON);
        }
        if ( _A2 < _A1 && _A2 < _A3 ) {
          tmp &= ~(1<<_B_ACCU2_ON);
        }
        if ( _A3 < _A1 && _A3 < _A2 ) {
          tmp &= ~(1<<_B_ACCU3_ON);
        }
        if ( _A1 == _A2 && _A1 < _A3 ) {
          tmp &= ~(1<<_B_ACCU1_ON);
          tmp &= ~(1<<_B_ACCU2_ON);
        }
        if ( _A1 == _A3 && _A1 < _A2 ) {
          tmp &= ~(1<<_B_ACCU1_ON);
          tmp &= ~(1<<_B_ACCU3_ON);
        }
        if ( _A2 == _A3 && _A2 < _A1 ) {
          tmp &= ~(1<<_B_ACCU2_ON);
          tmp &= ~(1<<_B_ACCU3_ON);
        }
        if ( _A1 == _A2 && _A1 == _A3 ) {
          tmp &= ~(1<<_B_ACCU1_ON);
          tmp &= ~(1<<_B_ACCU2_ON);
          tmp &= ~(1<<_B_ACCU3_ON);
        }
      }
      else { // Normaler Modus
        charge = 0;
        LED_off;
        tmp = 0b00000111;
        if ( _A1 >= _A2 && _A1 >= _A3 ) {
          tmp &= ~(1<<_B_ACCU1_ON);
          tmp |= (1<<_B_ACCU2_ON)|(1<<_B_ACCU3_ON);
        }
        if ( _A2 > _A1 && _A2 > _A3 ) {
          tmp &= ~(1<<_B_ACCU2_ON);
          tmp |= (1<<_B_ACCU1_ON)|(1<<_B_ACCU3_ON);
        }
        if ( _A3 > _A1 && _A3 > _A2 ) {
          tmp &= ~(1<<_B_ACCU3_ON);
          tmp |= (1<<_B_ACCU1_ON)|(1<<_B_ACCU2_ON);
        }
      }
      if ( (tmp & 0b00000111) == 0b00000111 )
        tmp &= 0b11111110;
      PORTB = tmp;
    }
  }

  if ( timer > 110 ) {
    timer = 0;
  }

  Time1OVF = 0;
  do {
    goToSleep();
  } while( Time1OVF != 1 );

}

void goToSleep(){
    set_sleep_mode( SLEEP_MODE_IDLE );//MCUCR  &= ~((1<<SM0)|(1<<SM1)); //b01000000;
    ADCSRA = 0;            // turn off ADC
    sleep_enable();
    sleep_cpu();
    sleep_disable();
    ADCSRA = 1;            // turn on ADC
}

uint16_t GetADC(uint8_t channel) {
  ADCSRA |= 0b10000000;   //ADC ein
  channel &= 0b00000111;
  ADMUX = ( 0b10000000 | channel );

  ADCSRA |= _BV( ADIE );
  set_sleep_mode( SLEEP_MODE_ADC );
  sleep_enable();
  do   {
    sei();
    sleep_cpu();
    cli();
  } while( ( (ADCSRA & (1<<ADSC)) != 0 ) );

  sleep_disable();
  sei();
  ADCSRA &= ~ _BV( ADIE );
  return( ADC );
}   
  

ISR(TIM1_OVF_vect) {    //This is the interrupt request
  timer++;
  Time1OVF = 1;
}

ISR(ADC_vect) {
}
