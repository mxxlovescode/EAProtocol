// EAprotocol.h
#ifndef EA_PROTOCOL_H
#define EA_PROTOCOL_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoLog.h> // Подключение ArduinoLog для логирования

// Константы протокола
#define EA_PROTOCOL_MAX_MESSAGE_LENGTH 256
#define EA_PROTOCOL_BUFFER_SIZE 256


class EAprotocol {
public:
    // Конструктор и деструктор
    EAprotocol(HardwareSerial &serialPort, char endOfMessage = '\n', unsigned long timeout = 1000, long baudRate = 115200);

    // Основной цикл обработки
    void tick();
void processMessage();
private:

    // Членские переменные
    HardwareSerial &_serial;
    char _endOfMessage;
    unsigned long _timeout;

    char _buffer[EA_PROTOCOL_BUFFER_SIZE];            // Буфер для хранения данных
    unsigned long _lastReceiveTime;                   // Время последнего принятого символа
};

#endif // EA_PROTOCOL_H
