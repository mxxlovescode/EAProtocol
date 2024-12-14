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
    if (_buffer[0] != '\0') {
            processMessage();
            memset(_buffer, 0, _bufferSize);
    }
}

void EAprotocol::registerCommand(const char* command, void (*handler)(const char*)) {
    
}

void EAprotocol::sendCommand(const String& command, const String& data) {
    _serial.print(COMMAND_MARKER);
    _serial.print(command);
    if (!data.isEmpty()) {
        _serial.print(':');
        _serial.print(data);
    }
    _serial.print(_endOfMessage);
}


void EAprotocol::readDataToBuffer() {
    unsigned long startMillis = millis();

    while (millis() - startMillis < _timeout) {     // Читаем данные, пока они доступны, или до истечения таймаута
        
        if (_serial.available() > 0) {
            char c = (char)_serial.read();

            // Добавляем символ в буфер, если есть место
            if (_currentBufferLength < _bufferSize - 1) {
                _buffer[_currentBufferLength] = c;
                _buffer[_currentBufferLength] = '\0'; // Обеспечиваем корректное завершение строки
            } else {
                Log.warning(F("Буфер переполнен. Обработка данных."));
                processMessage(); // Обрабатываем данные при переполнении
                _currentBufferLength = 0;
            }

            // Проверяем конец сообщения
            if (c == _endOfMessage) {
                Log.trace(F("Сообщение завершено символом конца. Чтение завершено."));
                break; // Завершаем чтение, если получен конец сообщения
            }
        }
    }
}

void EAprotocol::processMessage()
{
    mString(_buffer, 256);
    if (_buffer[0] == COMMAND_MARKER) {
        // Вытаскиваем саму комманду
        const char* command_position = std::strstr(_buffer, "=");

        if (command_position) {
            size_t commnand_length = command_position - _buffer +1;                         // Вычисляем длинну комманды
            size_t args_length = _currentBufferLength - (command_position - _buffer + 1);   // Вычисляем длинну аргументов
            
            Log.verboseln(F("Выделенная комманда [%s]"), strcpy(_buffer + 1, command_position);
            Log.verboseln(F("Аргументы: [%s]"), std::strcpy(_buffer + 1 + commnand_length, );
        }
        else {
            std::string ex_command(_buffer +1, _currentBufferLength);               // Тут передаем диапазон для экстракции
            Log.verboseln(F("Выделенная комманда [%s]"), ex_command);
        }

    }
    else {
        _handleLog();
    }

    memset(_buffer, 0, _bufferSize);
    _currentBufferLength = 0; // Сброс длины буфера после обработки
}

void EAprotocol::_handleLog()
{
    Log.noticeln(F("<---[%s]"), std::string(_buffer[1], _buffer[_currentBufferLength]));
}