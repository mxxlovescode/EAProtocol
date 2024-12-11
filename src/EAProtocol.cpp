// EAprotocol.cpp
#include "EAprotocol.h"

// Конструктор
EAprotocol::EAprotocol(HardwareSerial &serialPort, char endOfMessage, unsigned long timeout, long baudRate)
  : _serial(serialPort), _endOfMessage(endOfMessage), _timeout(timeout), _lastReceiveTime(0)
{
    Log.notice(F("Создан экземпляр EAprotocol с EOM='%c' и таймаутом=%lu мс\r\n"), _endOfMessage, _timeout);
    _serial.begin(baudRate);
    Log.verbose(F("EAprotocol инициализирован с baud rate: %lu\r\n"), baudRate);

    _buffer[0] = '\0'; // Инициализация буфера
}

// Основной цикл обработки
void EAprotocol::tick() {
    unsigned long currentMillis = millis();

    // Проверяем, есть ли доступные данные
    if (_serial.available() > 0) {
        unsigned long startMillis = currentMillis;

        // Читаем данные, пока они доступны, или до истечения таймаута
        while (millis() - startMillis < _timeout) {
            if (_serial.available() > 0) {
                char c = (char)_serial.read();
                _lastReceiveTime = millis(); // Обновляем время последнего принятого символа

                // Добавляем символ в буфер, если есть место
                if (strlen(_buffer) < sizeof(_buffer) - 1) {
                    size_t len = strlen(_buffer);
                    _buffer[len] = c;
                    _buffer[len + 1] = '\0';
                } else {
                    Log.warning(F("Буфер переполнен. Данные могут быть утеряны.\r\n"));
                    processMessage();
                }

                // Проверяем конец сообщения
                if (c == _endOfMessage) {
                    Log.notice(F("Сообщение завершено символом конца. Передаем на обработку.\r\n"));
                    processMessage();
                    return; // Сообщение обработано, выходим из функции
                }
            }
        }

    }
}

// Обработка принятого сообщения
void EAprotocol::processMessage() {
    if (strlen(_buffer) > 0) {
        Log.notice(F("Принято сообщение: %s\r\n"), _buffer);
        // Очистка буфера для нового сообщения
        memset(_buffer, 0, sizeof(_buffer));
        _lastReceiveTime = 0; // Сброс времени последнего символа
    } else {
        Log.warning(F("Буфер пуст. Нет данных для обработки.\r\n"));
    }
}
