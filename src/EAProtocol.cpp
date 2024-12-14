#include <EAProtocol.h>

EAprotocol::EAprotocol(HardwareSerial &serialPort, char endOfMessage, unsigned long timeout) : 
    _serial(serialPort), _endOfMessage(endOfMessage), _timeout(timeout), commandCount(0) {
    }

void EAprotocol::begin(){
    _serial.begin(115200);
    _mBuffer.clear();
}

EAprotocol::~EAprotocol() {
}

void EAprotocol::tick() {
    // Сначала читаем данные во внутренний буфер
    if (_serial.available() > 0) readDataToBuffer();

    // Если в буфере есть данные, обрабатываем их
    if (_mBuffer.length() > 0) processMessage();
}

void EAprotocol::registerCommand(const char* command_name, void (*handler)(const char*)) {
    if (commandCount < 10) {  // Проверка, есть ли место для новых команд
        commands[commandCount].command_name_hash = hashString(command_name);
        commands[commandCount].handler = handler;
        commandCount++;  // Увеличиваем счетчик команд
    }
}

void EAprotocol::executeCommand(const char* command) {
    uint32_t commandHash = hashString(command);  // Вычисляем хеш команды
    for (int i = 0; i < commandCount; i++) {
        if (commands[i].command_name_hash == commandHash) {
            commands[i].handler(command);  // Вызываем обработчик
            return;
        }
    }
    Log.warningln(F("Неизвестная комманда: [%s]"), command); // Если команда не найдена
}


void EAprotocol::sendCommand(const String& command, const String& data) {
}


void EAprotocol::readDataToBuffer() {
    unsigned long startMillis = millis();

    char c;

    while (millis() - startMillis < _timeout) {     // Читаем данные, пока они доступны, или до истечения таймаута
        
        c = _serial.read();

        if (c == -1 || c == '\r') {
            continue; // Игнорируем \r
        }
        // Проверяем конец сообщения
        if (c == _endOfMessage) {
            Log.traceln(F("Сообщение завершено символом конца. Чтение завершено. Принятых символов: [%d]"), _mBuffer.length());
            break; // Завершаем чтение, если получен конец сообщения
        }

        // Добавляем символ в буфер, если есть место
        if (_mBuffer.capacity() - _mBuffer.length() > 0) {
            _mBuffer.add(c); 
        } else {
            Log.warning(F("Буфер переполнен. Обработка данных."));
            processMessage(); // Обрабатываем данные при переполнении
        }
    }
}

void EAprotocol::processMessage()
{
    if (_mBuffer.startsWith(COMMAND_MARKER)) { // Если комманда
        
        _mBuffer.remove(0, 1);
        int command_lenght = _mBuffer.indexOf(COMMAND_DIVIDER, 0);
                
        if (command_lenght > 0) {
            char command_name[command_lenght];

            Log.verboseln(F("Буффер: [%s]. Позиция разделителя: %d"), _mBuffer.buf, command_lenght);
            
            _mBuffer.substring(0, command_lenght -1, command_name);
            _mBuffer.remove(0, command_lenght + 1);
            
            Log.verboseln(F("Определена комманда: [%s]"), command_name);
            executeCommand(command_name);
        }
        else executeCommand(_mBuffer.buf);
    }

    else _handleLog();

    // Очищаем буфер и счетчик.
    _mBuffer.clear();
}

char *EAprotocol::getBuff()
{
    return _mBuffer.buf;
}

void EAprotocol::_handleLog()
{
    Log.noticeln(F("<---[%s]"), _mBuffer.buf);
}

uint32_t EAprotocol::hashString(const char* str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash * 31) + *str++;
    }
    return hash;
}
