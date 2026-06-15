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


motor motorEsq(PIN_PWMR_ESQ, PIN_PWML_ESQ);
motor motorDir(PIN_PWMR_DIR, PIN_PWML_DIR);

PIDController pidEsq(KPesq, KIesq, KDesq); 
PIDController pidDir(KPdir, KIdir, KDdir);

EncoderISR encEsq(PIN_ENC_ESQ);
EncoderISR encDir(PIN_ENC_DIR);

// ==========================================
// COMUNICAÇÃO ENTRE TAREFAS (FreeRTOS)
// ==========================================
// Criamos uma "caixa de correio" para a velocidade.
QueueHandle_t filaVelocidade;

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

  for(;;) {
    accgyr_update();
    vTaskDelay((1000/FREQ_ACCGYR_HZ) / portTICK_PERIOD_MS); // Roda a 100Hz exatos
  }
}

void codigoTaskMag(void * parameter) {
  for(;;) {
    mag_update();
    vTaskDelay((1000/FREQ_MAG_HZ) / portTICK_PERIOD_MS); // Roda a 100Hz exatos
  }
}

void codigoTaskGPS(void * parameter) {
  gps_init();

  for(;;) {
    gps_update();
    vTaskDelay((1000/FREQ_GPS_HZ) / portTICK_PERIOD_MS); // Roda a 10Hz exatos
  }
}

void codigoTaskSonar(void * parameter) {
  sonar_init();

  for(;;) {
    sonar_update();
    vTaskDelay((1000/FREQ_SONAR_HZ) / portTICK_PERIOD_MS); // Roda a 20Hz exatos
  }
}

void codigoTaskSensores(void * parameter) {
  // 2. Loop infinito da tarefa
  for(;;) {
    // -> Transmitir dados para o ROS
    // Serial.println("---");
    accgyr_print_ros_format();
    mag_print_ros_format();
    gps_print_ros_format();
    sonar_print_ros_format();

    vTaskDelay((1000/FREQ_SENSORES_HZ) / portTICK_PERIOD_MS); 
  }
}



// ==========================================
// TAREFA 2: MOTORES & PID (Rodando no CORE 1)
// ==========================================
void codigoTaskMotores(void * parameter) {

  motorEsq.init();
  motorDir.init();
  encEsq.init();
  encDir.init();

  float vel_ref = 0.0; // RPM alvo

  for(;;) {

    float nova_vel;
    if (xQueueReceive(filaVelocidade, &nova_vel, 0) == pdTRUE) {
      vel_ref = nova_vel;
      Serial.print("[TASK MOTORES] Nova Velocidade Alvo: ");
      Serial.println(vel_ref);
    }
    // 1. Lê a velocidade de forma totalmente independente e paralela!
    float rpm_esq = encEsq.lerVelocidadeRPM(); 
    float rpm_dir = encDir.lerVelocidadeRPM();

    // 2. O PID calcula o PWM
    pidEsq.setSentido(vel_ref);
    pidDir.setSentido(vel_ref);
    int pwm_esq = pidEsq.controle(vel_ref, rpm_esq);
    int pwm_dir = pidDir.controle(vel_ref, rpm_dir);

    // Serial.print("[TASK MOTORES] PWM Esquerda: ");
    // Serial.print(pwm_esq);
    // Serial.print(" | PWM Direita: ");
    // Serial.println(pwm_dir);

    // 3. Aplica na ponte H
    motorEsq.acionaMotor(pwm_esq);
    motorDir.acionaMotor(pwm_dir);

    vTaskDelay((1000/FREQ_MOTORES_HZ) / portTICK_PERIOD_MS); // Roda a 100Hz exatos
  }
}

// ==========================================
// SETUP PRINCIPAL (O MAESTRO)
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("=== Iniciando Sistema (FreeRTOS) ===");

  // 1. Inicializa a Fila de Comunicação
  // Criamos espaço para guardar até 5 valores do tipo 'float'
  filaVelocidade = xQueueCreate(5, sizeof(float));

  // 2. Inicializa o Webserver (Geralmente ele roda de forma assíncrona no Core 0)
  webserver_init(WIFI_SSID, WIFI_PASS);

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