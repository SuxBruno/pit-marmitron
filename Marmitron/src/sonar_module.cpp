#include "sonar_module.h"
#include "pinout.h"
#include <Arduino.h>

// ==========================================
// VARIÁVEIS INTERNAS E VOLÁTEIS
// ==========================================
static float distance_m = -1.0;

// Variáveis que a interrupção vai modificar
static volatile unsigned long echo_start_us = 0;
static volatile unsigned long echo_duration_us = 0;
static volatile bool novo_dado_disponivel = false;

// Fechadura de proteção para o FreeRTOS
static portMUX_TYPE muxSonar = portMUX_INITIALIZER_UNLOCKED;

// ==========================================
// ROTINA DE INTERRUPÇÃO (ISR)
// ==========================================
// Esta função roda em nanossegundos quando o pino ECHO muda de estado
void IRAM_ATTR sonar_echo_isr() {
  unsigned long tempo_agora = micros();
  
  if (digitalRead(PIN_SONAR_ECHO) == HIGH) {
    // O pino subiu! O som começou a sair/viajar
    echo_start_us = tempo_agora;
  } 
  else {
    // O pino desceu! O eco voltou
    if (echo_start_us != 0) {
      echo_duration_us = tempo_agora - echo_start_us;
      novo_dado_disponivel = true;
    }
  }
}

// ==========================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ==========================================

bool sonar_init() {
  pinMode(PIN_SONAR_TRIG, OUTPUT);
  pinMode(PIN_SONAR_ECHO, INPUT);
  
  digitalWrite(PIN_SONAR_TRIG, LOW);
  
  // Configura a interrupção para disparar em QUALQUER mudança (Subida ou Descida)
  attachInterrupt(digitalPinToInterrupt(PIN_SONAR_ECHO), sonar_echo_isr, CHANGE);
  
  Serial.println("HC-SR04 Não-Bloqueante Inicializado.");
  return true;
}

void sonar_update() {
  // 1. ANTES de disparar um novo pulso, processamos o resultado do pulso ANTERIOR
  unsigned long duracao_copia = 0;
  bool dados_novos = false;

  // Seção crítica: copia as variáveis da ISR com segurança
  portENTER_CRITICAL(&muxSonar);
  dados_novos = novo_dado_disponivel;
  duracao_copia = echo_duration_us;
  novo_dado_disponivel = false; // Reseta o sinalizador
  portEXIT_CRITICAL(&muxSonar);

  if (dados_novos && duracao_copia < 30000) { // 30ms = ~5 metros máximo
    // Matemática do seu código antigo
    float distance_cm = (duracao_copia * 0.0343) / 2.0;
    distance_m = distance_cm / 100.0;
  } else {
    // Se não houve dado novo, significa que o som se perdeu no infinito (Timeout)
    distance_m = -1.0;
  }

  // 2. DISPARA O NOVO PULSO (Leva apenas 12 microssegundos)
  digitalWrite(PIN_SONAR_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_SONAR_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_SONAR_TRIG, LOW);
  
  // FIM! O código não espera o ECHO voltar. Ele sai da função imediatamente!
}

void sonar_print_ros_format() {
  Serial.println(distance_m, 4);
}