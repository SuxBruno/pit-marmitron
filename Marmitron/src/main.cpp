#include <Arduino.h>
#include "pinout.h"
#include "config.h"

#include "imu_module.h"
#include "gps_module.h"
#include "sonar_module.h"

#include "encoder_module.h"
#include "motor_module.h"
#include "ControlePID.h"

#include "webserver_module.h" 

#include "ctrl_velocidade.h"

motor motorEsq(PIN_PWMR_ESQ, PIN_PWML_ESQ);
motor motorDir(PIN_PWMR_DIR, PIN_PWML_DIR);

PIDController pidEsq(KPesq, KIesq, KDesq); 
PIDController pidDir(KPdir, KIdir, KDdir);

EncoderISR encEsq(PIN_ENC_ESQ, 0.9);
EncoderISR encDir(PIN_ENC_DIR, 0.9);

// ==========================================
// COMUNICAÇÃO ENTRE TAREFAS (FreeRTOS)
// ==========================================
// Criamos uma "caixa de correio" para a velocidade.
QueueHandle_t filaVelocidadeEsq;
QueueHandle_t filaVelocidadeDir;

// Identificadores das tarefas
TaskHandle_t TaskSensores;
TaskHandle_t TaskAccGyr;
TaskHandle_t TaskMag;
TaskHandle_t TaskGPS;
TaskHandle_t TaskSonar;
TaskHandle_t TaskMotores;

// ==========================================
// TAREFA 1: SENSORES (Rodando no CORE 1)
// ==========================================

void codigoTaskAccGyr(void * parameter) {
  imu_init();
  TickType_t tempoExecAnterior;
  const TickType_t periodoExec = (1000/FREQ_ACCGYR_HZ) / portTICK_PERIOD_MS;

  for(;;) {
    accgyr_update();
    vTaskDelayUntil(&tempoExecAnterior, periodoExec);
  }
}

void codigoTaskMag(void * parameter) {
  for(;;) {
    mag_update();
    vTaskDelay((1000/FREQ_MAG_HZ) / portTICK_PERIOD_MS); // Roda a 100Hz exatos
  }
}

void codigoTaskGPS(void * parameter) {
  //gps_init();

  for(;;) {
    //gps_update();
    vTaskDelay((1000/FREQ_GPS_HZ) / portTICK_PERIOD_MS); // Roda a 10Hz exatos
  }
}

void codigoTaskSonar(void * parameter) {
  //sonar_init();

  for(;;) {
    //sonar_update();
    vTaskDelay((1000/FREQ_SONAR_HZ) / portTICK_PERIOD_MS); // Roda a 20Hz exatos
  }
}

void codigoTaskSensores(void * parameter) {
  // 2. Loop infinito da tarefa
  TickType_t tempoExecAnterior;
  const TickType_t periodoExec = (1000/FREQ_SENSORES_HZ) / portTICK_PERIOD_MS;
  Serial.println("TaskSensores inicializada");
  for(;;) {
      // -> Transmitir dados para o ROS
      accgyr_print_ros_format();
      mag_print_ros_format();
      //gps_print_ros_format();
      //sonar_print_ros_format();
      vTaskDelayUntil(&tempoExecAnterior, periodoExec); 
  }
}



// ==========================================
// TAREFA 2: MOTORES & PID (Rodando no CORE 1)
// ==========================================
void codigoTaskMotores(void * parameter) {

  //motorEsq.init();
  //motorDir.init();
  //encEsq.init();
  //encDir.init();

  float vel_ref_esq = 0.0; // RPM alvo
  float vel_ref_dir = 0.0; 
  float rpm_esq = 0.0;
  float rpm_dir = 0.0;

  unsigned long t_atual, t_anterior;
  unsigned long periodo_amostragem = 10;
  for(;;) {
    t_atual = millis();
    float nova_vel_esq;
    float nova_vel_dir;
    if (xQueueReceive(filaVelocidadeEsq, &nova_vel_esq, 0) == pdTRUE) {
      vel_ref_esq = nova_vel_esq;
      //pidEsq.reset();
      //Serial.print("[TASK MOTORES] Nova Velocidade Alvo (Esquerda): ");
      //Serial.println(vel_ref_esq);
    }

    if (xQueueReceive(filaVelocidadeDir, &nova_vel_dir, 0) == pdTRUE) {
      vel_ref_dir = nova_vel_dir;
      //pidDir.reset();
      //Serial.print("[TASK MOTORES] Nova Velocidade Alvo (Direita): ");
      //Serial.println(vel_ref_dir);
    }

    //rpm_esq = encEsq.lerVelocidadeRPM(); 
    //rpm_dir = encDir.lerVelocidadeRPM();
    // 2. O PID calcula o PWM
    //Serial.println(vel_ref_esq);
    //pidEsq.setSentido(vel_ref_esq);
    //pidDir.setSentido(vel_ref_dir);
    //rpm_esq = encEsq.filtraVelocidade();
    //rpm_dir = encDir.filtraVelocidade();
    //int pwm_esq = pidEsq.controle(abs(vel_ref_esq), rpm_esq);
    //int pwm_dir = pidDir.controle(abs(vel_ref_dir), rpm_dir);

    //Serial.print("[TASK MOTORES] PWM Esquerda: ");
    //Serial.print(pwm_esq);
    //Serial.print(" | PWM Direita: ");
    //Serial.println(pwm_dir);

    // 3. Aplica na ponte H
    //motorEsq.acionaMotor(pwm_esq);
    //motorDir.acionaMotor(pwm_dir);

    vTaskDelay((1000/FREQ_MOTORES_HZ) / portTICK_PERIOD_MS);
  }
}

// ==========================================
// SETUP PRINCIPAL (O MAESTRO)
// ==========================================
void setup() {
  Serial.begin(921600);
  delay(2000); 
  Serial.println("=== Iniciando Sistema (FreeRTOS) ===");

  // 1. Inicializa a Fila de Comunicação
  // Criamos espaço para guardar até 5 valores do tipo 'float'
  filaVelocidadeEsq = xQueueCreate(5, sizeof(float));
  filaVelocidadeDir = xQueueCreate(5, sizeof(float));

  // 2. Inicializa o Webserver (Geralmente ele roda de forma assíncrona no Core 0)
  //webserver_init(WIFI_SSID, WIFI_PASS);

  // 3. Cria e lança a Tarefa dos Sensores no CORE 1
  xTaskCreatePinnedToCore(
    codigoTaskSensores,  /* Função da tarefa */
    "TaskSensores",      /* Nome para debug */
    10000,               /* Memória reservada (Stack) */
    NULL,                /* Parâmetros extras */
    1,                   /* Prioridade Normal (1) */
    &TaskSensores,       /* Handle da tarefa */
    1);                  /* Fixado no CORE 1 */

  xTaskCreatePinnedToCore(
    codigoTaskAccGyr, 
    "TaskAccGyr", 
    10000, 
    NULL, 
    2,                   /* Prioridade Normal (2) */
    &TaskAccGyr, 
    1);                  /* Fixado no CORE 1 */

  xTaskCreatePinnedToCore( 
    codigoTaskMag, 
    "TaskMag", 
    10000, 
    NULL, 
    1,                   /* Prioridade Normal (1) */
    &TaskMag, 
    1);                  /* Fixado no CORE 1 */

  xTaskCreatePinnedToCore(
    codigoTaskGPS, 
    "TaskGPS", 
    10000, 
    NULL, 
    3,                   /* Prioridade Normal (3) */
    &TaskGPS, 
    1);                  /* Fixado no CORE 1 */

  xTaskCreatePinnedToCore(
    codigoTaskSonar, 
    "TaskSonar", 
    10000, 
    NULL, 
    3,                   /* Prioridade Normal (3) */
    &TaskSonar, 
    1);                  /* Fixado no CORE 1 */

  // 4. Cria e lança a Tarefa dos Motores no CORE 1
  xTaskCreatePinnedToCore(
    codigoTaskMotores, 
    "TaskMotores", 
    10000, 
    NULL, 
    4,                   /* Prioridade ALTA (4) - Motores são críticos! */
    &TaskMotores, 
    1);                  /* Fixado no CORE 1 */

  Serial.println("Tarefas criadas. O FreeRTOS assumiu o controle.");
}

void loop() {
  // O loop padrão virou inútil porque nós criamos nossas próprias tarefas.
  // Deletamos a tarefa padrão do loop para devolver essa memória RAM ao ESP32.
  vTaskDelete(NULL);
}