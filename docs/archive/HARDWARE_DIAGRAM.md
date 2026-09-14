# Diagramma Hardware: ESP32-S3 Aero-Edu Swarm (Definitivo UART)

Questo diagramma riflette l'uso del modulo integrato Optical Flow + ToF via UART e il monitoraggio batteria.

```mermaid
graph TD
    %% --- POWER SUBSYSTEM ---
    subgraph Power
        LIPO(LiPo_1S) -- BT2.0 --> SW(Switch)
        SW -- VBAT_4.2V --> VCC(XIAO_VUSB_Pin)
        LIPO -- GND --> GND(Common_GND)
        
        VCC -- Internal_LDO --> RAIL_3V3(XIAO_3V3_Pin)
        
        VCC -- Filter --> CAP(Cap_470uF)
        VCC -- Divider --> RDIV(Resistor_Divider)
        RDIV -- Half_Vbat --> PIN_D8(Pin_D8_Sense)
        
        VCC -- High_Current --> MOT_PWR(Motors_Power)
        
        RAIL_3V3 -- 40mA --> FLOW_PWR(Optical_Flow_VCC)
        RAIL_3V3 -- 5mA --> IMU_PWR(IMU_VCC)
    end

    %% --- MAIN CONTROLLER ---
    subgraph FC
        D0(Pin_D0) -- PWM --> M1(Motor_1)
        D1(Pin_D1) -- PWM --> M2(Motor_2)
        D2(Pin_D2) -- PWM --> M3(Motor_3)
        D3(Pin_D3) -- PWM --> M4(Motor_4)
        
        D4(Pin_D4) --- I2C_BUS
        D5(Pin_D5) --- I2C_BUS
        
        D6(Pin_D6_TX) -- UART_TX --> RX_MOD(Module_RX)
        D7(Pin_D7_RX) -- UART_RX --> TX_MOD(Module_TX)
        
        D9(Pin_D9) -- PWM --> BUZ(Buzzer)
        D10(Pin_D10) -- GPIO --> LED(Debug_LED)
    end

    %% --- SENSORS ---
    subgraph Sensors
        I2C_BUS --- SDA(IMU_SDA)
        I2C_BUS --- SCL(IMU_SCL)
        
        RX_MOD --- FLOW(Optical_Flow_Module)
        TX_MOD --- FLOW
        
        FLOW_PWR --- FLOW
        IMU_PWR --- SDA
    end
```

## PINOUT Finale XIAO ESP32S3
1.  **D0, D1, D2, D3:** Motori (PWM)
2.  **D4, D5:** Bus I2C (IMU - GY-521)
3.  **D6, D7:** Bus UART1 (Flow/ToF - Matek 3901-L0X)
4.  **D8 (A8):** V-Sense (Analog Read Batteria)
5.  **D9:** Buzzer (Audio Feedback)
6.  **D10:** LED status (singolo colore) via R 330Ω.
7.  **3V3 (Output):** Alimentazione SENSORI (IMU + Flow). **Max 200mA.** (Carico attuale: ~45mA).
8.  **VUSB (Input):** Ingresso alimentazione da Batteria 1S (via Switch). **NON collegare USB e batteria contemporaneamente.** **NON COLLEGARE SENSORI QUI.**
9.  **GND:** Massa comune.
