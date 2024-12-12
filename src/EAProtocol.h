// EAprotocol.h
#ifndef EA_PROTOCOL_H
#define EA_PROTOCOL_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoLog.h> // Подключение ArduinoLog для логирования

class EAprotocol {
public:
    // Конструктор и деструктор
    EAprotocol(HardwareSerial &serialPort, char endOfMessage = '\n', unsigned long timeout = 1000, size_t bufferSize = 256);

    ~EAprotocol();

    // Основной цикл обработки
    void tick();
    void readDataToBuffer();
    void processMessage();

private:

    // Членские переменные
    HardwareSerial &_serial;
    char _endOfMessage;
    unsigned long _timeout;

    size_t _bufferSize;
    char *_buffer;            // Буфер для хранения данных
    unsigned long _lastReceiveTime;                   // Время последнего принятого символа
};

#endif // EA_PROTOCOL_H
