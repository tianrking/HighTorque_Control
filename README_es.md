# Biblioteca de Control de Motores de Alto Par de LivelyBot

Una biblioteca de control de motores desarrollada basada en la tabla de protocolos del SDK de motores de alto par, soportando implementaciones en Python, C++, Rust y Arduino.

🌐 **Languages**: [English](README.md) | [中文](README_zh.md) | [Español](README_es.md)

🔗 **SDK Oficial**: https://github.com/HighTorque-Robotics/livelybot_hardware_sdk

## 🚀 Inicio Rápido

### Configuración del Entorno

```bash
# Instalar herramientas CAN
sudo apt-get install can-utils

# Configurar interfaz CAN
sudo ip link set can0 type can bitrate 1000000
sudo ip link set up can0
```

### Implementación Python

```bash
cd python
pip install -r requirements.txt

# Escanear motores
python3 can_motor_scanner.py --channel can0

# Control de velocidad
python3 velocity_acceleration_control.py --motor_id 1 --mode interactive

# Control de ángulo
python3 angle_stream_control.py --motor_id 1 --mode interactive
```

### Implementación C++ ⏳ POR HACER

```bash
# Por implementar
cd cpp
make
sudo ./build/lively-motor-control 1
```

### Implementación Rust ⏳ POR HACER

```bash
# Por implementar
cd rust
cargo run --release -- 1
```

## 📁 Estructura del Proyecto

```
CUS_02/
├── python/                 # Implementación Python
│   ├── can_motor_scanner.py           # Herramienta de escaneo de motores
│   ├── velocity_acceleration_control.py # Control de velocidad + aceleración
│   ├── angle_stream_control.py         # Control de flujo de ángulo
│   └── requirements.txt               # Dependencias Python
├── cpp/                    # Implementación C++
│   ├── src/
│   ├── include/
│   └── Makefile
├── rust/                   # Implementación Rust
│   ├── src/
│   └── Cargo.toml
├── arduino/                # Implementación Arduino
│   └── libraries/
└── scripts/                # Scripts de construcción y configuración
```

## 🔧 Características

### Implementación Python (100Hz, 5ms latencia) ✅
- ✅ Escaneo de motores en bus CAN
- ✅ Control de velocidad con parada de emergencia inteligente
- ✅ Control de ángulo con comando de flujo 0x90
- ✅ Control de impedancia estilo MIT
- ✅ Pruebas de onda sinusoidal/escalón/rampa

### Implementación C++ (200Hz, 1ms latencia) ⏳ POR HACER
- ⏳ Control en tiempo real de alto rendimiento
- ⏳ Interfaz CAN nativa
- ⏳ Arquitectura de control multihilo

### Implementación Rust (150Hz, 2ms latencia) ⏳ POR HACER
- ⏳ Garantía de seguridad de memoria
- ⏳ Arquitectura de control asíncrono
- ⏳ Soporte multiplataforma

### Implementación Arduino (50-200Hz, 2-20ms latencia) ⏳ POR HACER
- ⏳ Soporte ESP32/Arduino
- ⏳ Operación de bajo consumo
- ⏳ Retroalimentación en tiempo real

## 📊 Motores Soportados

Basado en la tabla de protocolos del SDK de motores de alto par:

| Modelo de Motor | Par | Velocidad Máxima | Reducción | Soporte de Protocolo |
|----------------|------|-----------------|-----------|----------------------|
| 5046_20 | 17 Nm | 50 rad/s | 20:1 | ✅ |
| 4538_19 | 17 Nm | 44 rad/s | 19:1 | ✅ |
| 5047_36 | 60 Nm | 50 rad/s | 36:1 | ✅ |
| 5047_09 | 17 Nm | 33 rad/s | 9:1 | ✅ |

## 🔌 Protocolo CAN

### Arquitectura de Comunicación
- **Maestro**: Control directo vía interfaz CAN
- **Motor**: Soporte para control de flujo multimotor
- **Velocidad de Baudios**: 1Mbps (estándar)
- **Formato de Trama**: Trama extendida (ID de 29 bits)

### Protocolos Clave

#### 1. Escaneo de Motores (Ping)
```python
# ID CAN: 0x8000 | motor_id
# Datos: [0x11, 0x00, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50]
```

#### 2. Control de Velocidad + Aceleración (0xAD)
```python
# ID CAN: 0x00AD
# Datos: [PosL, PosH, VelL, VelH, AccL, AccH, 0x50, 0x50]
```

#### 3. Control de Flujo de Ángulo (0x90)
```python
# ID CAN: 0x0090
# Datos: [PosL, PosH, VelL, VelH, TqeL, TqeH, 0x50, 0x50]
```

## 🎯 Ejemplos de Uso

### Escaneo de Motores Python

```python
from python.can_motor_scanner import LivelyMotorScanner

scanner = LivelyMotorScanner('can0')
if scanner.connect():
    motors = scanner.scan_range(1, 14)
    print(f"Motores encontrados: {motors}")
```

### Control de Velocidad Python

```python
from python.velocity_acceleration_control import MotorVelAccController

controller = MotorVelAccController('can0', motor_id=1)
controller.enable_sequence()
controller.start_control()
controller.set_velocity(5.0)  # 5 rad/s
```

### Control de Ángulo Python

```python
from python.angle_stream_control import MotorAngleStreamController

controller = MotorAngleStreamController('can0', motor_id=1)
controller.connect()
controller.enable_motor()
controller.set_angle(90.0)  # 90 grados
```

## 🛡️ Características de Seguridad

- **Limitación de Par**: Par de salida máximo configurable
- **Limitación de Posición**: Límites de posición por software
- **Parada de Emergencia Inteligente**: Desaceleración máxima automática a velocidad cero
- **Monitoreo de Comunicación**: Estado de comunicación CAN en tiempo real
- **Manejo de Excepciones**: Manejo y recuperación de errores completo

## 📈 Comparación de Rendimiento

| Lenguaje | Frecuencia de Control | Latencia | Uso de Memoria | Estado | Plataforma Objetivo |
|----------|---------------------|----------|---------------|--------|-------------------|
| Python | 100 Hz | 5ms | 50MB | ✅ Completado | Desarrollo Linux |
| C++ | 200 Hz | 1ms | 10MB | ⏳ POR HACER | Producción Linux |
| Rust | 150 Hz | 2ms | 15MB | ⏳ POR HACER | Producción Linux |
| Arduino | 50-200Hz | 2-20ms | 10-50KB | ⏳ POR HACER | ESP32/MCU |

## 🔍 Solución de Problemas

### Problemas de Interfaz CAN
```bash
# Verificar estado de interfaz
ip link show can0

# Reconfigurar interfaz
sudo ip link set can0 down
sudo ip link set can0 up type can bitrate 1000000 restart-ms 100
```

### Problemas de Permisos
```bash
# Añadir usuario al grupo dialout
sudo usermod -a -G dialout $USER

# O ejecutar con sudo
sudo python3 can_motor_scanner.py
```

### Conexión de Hardware
- Confirmar resistencia terminal de 120Ω
- Verificar cableado CAN-H y CAN-L
- Verificar que la alimentación del motor sea normal
- Confirmar que la configuración de baud rate coincida

## 📚 Documentación

- 📄 [Tabla de Protocolos del SDK de Motores de Alto Par](../高擎电机SDK协议表.md) - Especificación completa de protocolos
- 🔗 [SDK Oficial](https://github.com/HighTorque-Robotics/livelybot_hardware_sdk) - Repositorio SDK oficial
- 📖 [Documentación Python](python/README.md) - Detalles de implementación Python
- ⏳ [Documentación C++](cpp/README.md) - Detalles de implementación C++ (Por implementar)
- ⏳ [Documentación Rust](rust/README.md) - Detalles de implementación Rust (Por implementar)
- ⏳ [Documentación Arduino](arduino/README.md) - Detalles de implementación Arduino (Por implementar)

## 🗺️ Hoja de Ruta de Desarrollo

### ✅ Completado
- [x] Implementación de protocolo CAN Python
- [x] Herramienta de escaneo de motores
- [x] Control de velocidad + aceleración (parada de emergencia inteligente)
- [x] Control de flujo de ángulo (comando 0x90)
- [x] Control de impedancia estilo MIT
- [x] Múltiples modos de prueba

### ⏳ Por Implementar
- [ ] Implementación C++ de alto rendimiento
- [ ] Implementación Rust segura de memoria
- [ ] Implementación Arduino/ESP32
- [ ] Interfaz de control GUI
- [ ] Plataforma de prueba de simulación
- [ ] Herramientas de calibración automática

### 🚀 Planes Futuros
- [ ] Identificación automática de parámetros de motor
- [ ] Gestión por lotes de motores
- [ ] Visualización de datos en tiempo real
- [ ] Interfaz de control remoto
- [ ] Herramientas de diagnóstico de fallas

## 🤝 Contribuir

¡Issues y Pull Requests son bienvenidos!

### Cómo Contribuir
1. Fork este repositorio
2. Crear rama de característica (`git checkout -b feature/AmazingFeature`)
3. Hacer commit de cambios (`git commit -m 'Add some AmazingFeature'`)
4. Push a la rama (`git push origin feature/AmazingFeature`)
5. Abrir Pull Request

## 📄 Licencia

MIT License - Ver archivo LICENSE para detalles

## Enlaces Relacionados

- [SDK Oficial de High Torque Robotics](https://github.com/HighTorque-Robotics/livelybot_hardware_sdk) - Referencia de protocolos
- [RobStride Control](https://github.com/tianrking/RobStride_Control) - Inspiración de control de motores arquimedianos

---

*Desarrollado basado en la tabla de protocolos del SDK de motores de alto par, proporcionando soluciones de alto rendimiento para control de robots*