#pragma once

// ==========================================
// REDE WI-FI
// ==========================================
// Usamos 'const char*' para textos
const char* const WIFI_SSID = "Sux";
const char* const WIFI_PASS = "Bruno123";

// ==========================================
// FREQUÊNCIA DE ATUALIZAÇÃO DOS SENSORES
// ==========================================
constexpr int FREQ_ACCGYR_HZ = 100; // 100Hz é um bom padrão para IMUs
constexpr int FREQ_MAG_HZ = 100; // 100Hz é um bom padrão para magnetômetros
constexpr int FREQ_GPS_HZ = 10; // 10Hz é um bom padrão para GPS
constexpr int FREQ_SONAR_HZ = 20; // 20Hz é um bom padrão para sonar
constexpr int FREQ_SENSORES_HZ = 20; // Frequência de transmissão dos dados para o ROS (20Hz é um bom padrão)
constexpr int FREQ_MOTORES_HZ = 100; // 100Hz é um bom padrão para controle de motores

// ==========================================
// SINTONIA DO PID (RODAS)
// ==========================================
// Usamos 'constexpr float' para números, pois é mais seguro e rápido que o #define
constexpr float KPesq = 2.0;
constexpr float KIesq = 0.5;
constexpr float KDesq = 0.1;
constexpr float KPdir = 2.0;
constexpr float KIdir = 0.5;
constexpr float KDdir = 0.1;

// ==========================================
// PARÂMETROS FÍSICOS DO ROBÔ (Exemplos úteis)
// ==========================================
// constexpr float RAIO_DA_RODA_METROS = 0.033; 
// constexpr float DISTANCIA_ENTRE_RODAS = 0.15; // Para a odometria
// constexpr int FUROS_DO_ENCODER = 40;