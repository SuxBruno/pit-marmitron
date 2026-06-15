#include "motor_module.h"

motor::motor(int pwmr, int pwml){
  // this->enable = enablePin;
  this->rpwm = pwmr;
  this->lpwm = pwml;
}

void motor::init(){
  // pinMode(enable, OUTPUT);
  pinMode(rpwm, OUTPUT);
  pinMode(lpwm, OUTPUT);

  // digitalWrite(enable, HIGH);

  analogWrite(rpwm, 0);
  analogWrite(lpwm, 0);
}

void motor::acionaMotor(int pwm){
  if(pwm > 0){
    analogWrite(lpwm, 0);
    analogWrite(rpwm, pwm);
  }
  else if(pwm < 0){
    analogWrite(rpwm, 0);
    analogWrite(lpwm, abs(pwm));
  }else{
    analogWrite(rpwm, 0);
    analogWrite(lpwm, 0);
  }
}
