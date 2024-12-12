#include <EAProtocol.h>

EAprotocol::EAprotocol(HardwareSerial &serialPort, char endOfMessage, unsigned long timeout, size_t bufferSize) : 
    _serial(serialPort), _endOfMessage(endOfMessage), _timeout(timeout), _bufferSize(bufferSize){
    _serial.begin(115200);
    _buffer = new char[_bufferSize]; // Выделяем память
    memset(_buffer, 0, _bufferSize); // Инициализируем буфер
}

EAprotocol::~EAprotocol() {
    delete[] _buffer; // Освобождаем память при уничтожении объекта
}

void EAprotocol::tick() {
    // Сначала читаем данные во внутренний буфер
    readDataToBuffer();

    // Если в буфере есть данные, обрабатываем их
    if (strlen(_buffer) > 0) {
        processMessage();
        _buffer[0] = '\0'; // Сбрасываем только указатель после обработки
    }
}

void EAprotocol::readDataToBuffer() {
    unsigned long startMillis = millis();
    size_t bufferLength = strlen(_buffer); // Отслеживаем длину буфера

    // Читаем данные, пока они доступны, или до истечения таймаута
    while (millis() - startMillis < _timeout) {
        if (_serial.available() > 0) {
            char c = (char)_serial.read();
            _lastReceiveTime = millis(); // Обновляем время последнего принятого символа

            // Добавляем символ в буфер, если есть место
            if (bufferLength < _bufferSize - 1) {
                _buffer[bufferLength++] = c;
                _buffer[bufferLength] = '\0'; // Обеспечиваем корректное завершение строки
            } else {
                Log.warning(F("Буфер переполнен. Обработка данных."));
                processMessage(); // Обрабатываем данные при переполнении
                memset(_buffer, 0, _bufferSize);
                bufferLength = 0;
            }

            // Проверяем конец сообщения
            if (c == _endOfMessage) {
                Log.notice(F("Сообщение завершено символом конца. Чтение завершено."));
                break; // Завершаем чтение, если получен конец сообщения
            }
        }
    }

    if (bufferLength > 0 && millis() - startMillis >= _timeout) {
        Log.warning(F("Таймаут истек. Обработка данных."));
        processMessage(); // Обрабатываем данные по таймауту
        memset(_buffer, 0, _bufferSize);
    }
}

void EAprotocol::processMessage()
{
    Serial.println(_buffer);
}
