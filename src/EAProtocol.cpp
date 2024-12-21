#include <EAProtocol.h>

// Конструктор класса EAprotocol
// Инициализирует последовательный порт, символ окончания сообщения и таймаут
EAprotocol::EAprotocol(HardwareSerial &serialPort, char endOfMessage, unsigned long timeout) : 
    _serial(serialPort), _endOfMessage(endOfMessage), _timeout(timeout), commandCount(0) {
        Log.verboseln(F("Текущий счетчик комманд [%d]"), commandCount);
}

// Метод для инициализации последовательного порта и очистки буфера
void EAprotocol::begin(){
    _serial.begin(115200);  // Устанавливаем скорость передачи данных 115200 бод
    _mBuffer.clear();       // Очищаем внутренний буфер
}

// Деструктор класса EAprotocol
EAprotocol::~EAprotocol() {
    
}

// Основной метод, который вызывается для обработки входящих данных
void EAprotocol::tick() {
    // Сначала читаем данные во внутренний буфер
    if (_serial.available() > 0) readDataToBuffer();

    // Если в буфере есть данные, обрабатываем их
    if (_mBuffer.length() > 0) processMessage();
}

// Регистрация команды с обработчиком
// command_name - имя команды, handler - указатель на функцию-обработчик
void EAprotocol::registerCommand(const char *commandName, void (*handler)(char *command_name, const char **args, const int *argCount))
{
     if (commandCount < MAX_NUMBER_OF_COMMAND) {  // Проверка, есть ли место для новых команд
        commands[commandCount].command_name_hash = hashString(commandName);  // Хешируем имя команды
        commands[commandCount].handler = handler;  // Сохраняем обработчик
        commandCount++;  // Увеличиваем счетчик команд
        Log.verboseln(F("Зарегистрированна команда [%s], hash - [%l]\nВсего комманд [%d]"), commandName, hashString(commandName), commandCount);
    }
}

// Обновленный метод чтения данных из последовательного порта во внутренний буфер
void EAprotocol::readDataToBuffer() {
    unsigned long startMillis = millis();
    char c;
    bool receivingMessage = false;

    while (millis() - startMillis < _timeout) {
        if (_serial.available() > 0) {
            c = _serial.read();

            if (c == EAPR_START_OF_MESSAGE) {
                // Начало нового сообщения
                _mBuffer.clear();
                receivingMessage = true;
                Log.traceln(F("Получен маркер начала сообщения."));
                continue;
            }

            if (c == EAPR_COMPLETE_MESSAGE) {
                // Конец сообщения
                receivingMessage = false;
                Log.noticeln(F("Сообщение полностью получено: [%s]"), _mBuffer.buf);
                break;
            }

            if (receivingMessage) {
                if (c == EAPR_END_OF_MESSAGE) {
                    // Завершение текущего пакета
                    Log.verboseln(F("Получен пакет: [%s]"), _mBuffer.buf);

                    // Отправляем подтверждение
                    _serial.write('A');
                    Log.traceln(F("Отправлено подтверждение пакета."));

                    startMillis = millis(); // Сбрасываем таймер
                } else {
                    if (_mBuffer.capacity() - _mBuffer.length() > 0) {
                        _mBuffer.add(c); // Сохраняем только полезные данные
                        startMillis = millis(); // Сбрасываем таймер при каждом новом символе
                    } else {
                        Log.warningln(F("Буфер переполнен. Принятые данные: [%s]"), _mBuffer.buf);
                        _serial.write('E'); // Сообщаем об ошибке
                        break;
                    }
                }
            }
        }
    }

    if (receivingMessage) {
        Log.errorln(F("Ошибка: Сообщение не завершено в установленное время."));
    }
}

//Выполнение комманды по ее имени
void EAprotocol::executeCommand(char *command, char **_args, const int *_args_amount)
{
    
    // Для дебага. Посимвольный вывод комманды.
    // for (const char *p = command; *p; p++) {
    //    Log.verboseln(F("[%c:%02X] "), *p, (unsigned char)*p);
    //}

    uint32_t commandHash = hashString(command);  // Вычисляем хеш команды
    Log.verboseln(F("Исполняем комманду [%s], ХЭШ: [%l]"), command, commandHash);
    for (int i = 0; i < commandCount; i++) {
        if (commands[i].command_name_hash == commandHash) {
            commands[i].handler(command, (const char**)_args, _args_amount);  // Вызываем обработчик
            return;
        }
    }
    Log.warningln(F("Неизвестная комманда: [%s]"), command); // Если команда не найдена
}


// Обработка сообщения из буфера
void EAprotocol::processMessage() {
    // Проверяем, начинается ли сообщение с маркера команды
    if (_mBuffer.startsWith(COMMAND_MARKER)) { 
        
        if (_mBuffer.endsWith(COMMAND_DIVIDER)) _mBuffer.truncate(1); // Чистим последний символ при необходимости
        _mBuffer.remove(0, 1);  // Удаляем маркер команды из буфера
        Log.verboseln(F("Длина: %d"), _mBuffer.length());

        _mBuffer.updateLength();
        Log.verboseln(F("Длина: %d"), _mBuffer.length());

        int command_lenght = _mBuffer.indexOf((char *)COMMAND_DIVIDER, 0);  // Находим разделитель команды
        if (command_lenght > 0) {
            char command_name[command_lenght + 1];  // Создаем буфер для имени команды  
            _mBuffer.substring(0, command_lenght - 1, command_name);  // Извлекаем имя команды
            _mBuffer.remove(0, command_lenght + 1);  // Удаляем обработанную часть из буфера
            char* _args[_mBuffer.splitAmount(COMMAND_DIVIDER[0])];
            const int _args_amount = _mBuffer.split(_args, COMMAND_DIVIDER[0]);
                        
            Log.verboseln(F("Определена комманда: [%s], определено аргументов [%d]."), command_name, _args_amount);
            executeCommand(command_name, _args, &_args_amount);  // Выполняем команду
        }
        else executeCommand(_mBuffer.buf, nullptr, 0);  // Выполняем команду, если разделитель не найден
    } else {
        _handleLog();  // Если это не команда, логируем сообщение
    }

    // Очищаем буфер после обработки
    _mBuffer.clear();
}

// Возвращает содержимое буфера
char *EAprotocol::getBuff() {
    return _mBuffer.buf;
}

// Логирование сообщений, не являющихся командами
void EAprotocol::_handleLog() {
    Log.noticeln(F("<---[%s]->"), _mBuffer.buf);
}

// Простейший алгоритм хеширования строки
uint32_t EAprotocol::hashString(const char* str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash * 31) + *str++;  // Умножаем текущий хеш на 31 и добавляем ASCII код символа
    }
    return hash;
}

// Реализация метода отправки данных с учетом разделения на пакеты
void EAprotocol::sendCommand(const char* message) {
    size_t messageLength = strlen(message);

    if (messageLength > MBUFFER_SIZE) {
        Log.errorln(F("Ошибка: Размер данных превышает допустимый предел 256 байт."));
        return;
    }

    // Добавляем маркеры начала и конца сообщения
    _serial.write(EAPR_START_OF_MESSAGE);
    Log.traceln(F("Отправлен маркер начала сообщения."));

    size_t start = 0;
    while (start < messageLength) {
        // Учитываем символ конца сообщения и символ окончания строки
        size_t remaining = messageLength - start;
        size_t packetSize = (remaining < (MAX_PACKET_SIZE - 2)) ? remaining : (MAX_PACKET_SIZE - 2);

        char packet[MAX_PACKET_SIZE]; // Размер пакета равен MAX_PACKET_SIZE
        strncpy(packet, message + start, packetSize);
        packet[packetSize] = EAPR_END_OF_MESSAGE; // Завершаем пакет символом конца сообщения
        packet[packetSize + 1] = '\0'; // Завершаем строку нулевым символом

        _serial.println(packet);
        Log.verboseln(F("Отправлен пакет: [%s]"), packet);

        // Ожидаем подтверждения от партнера
        unsigned long startMillis = millis();
        bool ackReceived = false;

        while (millis() - startMillis < _timeout) {
            if (_serial.available() > 0) {
                char ack = _serial.read();
                if (ack == 'A') { // Подтверждение от партнера
                    ackReceived = true;
                    break;
                }
            }
        }

        if (!ackReceived) {
            Log.errorln(F("Ошибка: Не получено подтверждение от партнера для пакета."));
            return;
        }

        start += packetSize;
    }

    _serial.write(EAPR_COMPLETE_MESSAGE);
    Log.traceln(F("Отправлен маркер конца сообщения."));

    Log.noticeln(F("Все пакеты успешно отправлены. Полное сообщение: [%s]"), message);
}