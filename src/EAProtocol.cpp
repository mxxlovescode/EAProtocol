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
void EAprotocol::registerCommand(const char *commandName, void (*handler)(const char *command_name, const char **args, const int *argCount))
{
     if (commandCount < MAX_NUMBER_OF_COMMAND) {  // Проверка, есть ли место для новых команд
        commands[commandCount].command_name_hash = hashString(commandName);  // Хешируем имя команды
        commands[commandCount].handler = handler;  // Сохраняем обработчик
        commandCount++;  // Увеличиваем счетчик команд
        Log.verboseln(F("Зарегистрированна команда [%s], hash - [%l]\nВсего комманд [%d]"), commandName, hashString(commandName), commandCount);
    }
}


// Отправка команды и данных (реализация пока отсутствует)
void EAprotocol::sendCommand(const String& command, const String& data) {
}

// Чтение данных из последовательного порта во внутренний буфер
void EAprotocol::readDataToBuffer() {
    unsigned long startMillis = millis();  // Начальное время для отслеживания таймаута

    char c;  // Переменная для хранения считанного символа

    // Читаем данные, пока они доступны, или до истечения таймаута
    while (millis() - startMillis < _timeout) {     
        c = _serial.read();  // Читаем один символ

        if (c == -1 || c == '\r') {
            continue; // Игнорируем символы \r или ошибку чтения
        }
        
        // Проверяем конец сообщения
        if (c == _endOfMessage) {
            Log.traceln(F("Сообщение завершено символом конца. Чтение завершено. Принятых символов: [%d]"), _mBuffer.length());
            break; // Завершаем чтение, если получен конец сообщения
        }

        // Добавляем символ в буфер, если есть место
        if (_mBuffer.capacity() - _mBuffer.length() > 0) {
            _mBuffer.add(c); 
            startMillis = millis();  // Сбрасываем таймер при каждом новом символе
        } else {
            Log.warning(F("Буфер переполнен. Обработка данных."));
            processMessage(); // Обрабатываем данные при переполнении
        }
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
            commands[i].handler(command, _args, _args_amount);  // Вызываем обработчик
            return;
        }
    }
    Log.warningln(F("Неизвестная комманда: [%s]"), command); // Если команда не найдена
}


// Обработка сообщения из буфера
void EAprotocol::processMessage() {
    // Проверяем, начинается ли сообщение с маркера команды
    if (_mBuffer.startsWith(COMMAND_MARKER)) { 
        _mBuffer.remove(0, 1);  // Удаляем маркер команды из буфера
        int command_lenght = _mBuffer.indexOf(COMMAND_DIVIDER, 0);  // Находим разделитель команды
                
        if (command_lenght > 0) {
            char command_name[command_lenght + 1];  // Создаем буфер для имени команды
           
            _mBuffer.substring(0, command_lenght - 1, command_name);  // Извлекаем имя команды
            _mBuffer.remove(0, command_lenght + 1);  // Удаляем обработанную часть из буфера
            char* _args[_mBuffer.splitAmount(COMMAND_DIVIDER)];
            const int _args_amount = _mBuffer.split(_args, COMMAND_DIVIDER);
                        
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
    Log.noticeln(F("<---[%s]"), _mBuffer.buf);
}

// Простейший алгоритм хеширования строки
uint32_t EAprotocol::hashString(const char* str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash * 31) + *str++;  // Умножаем текущий хеш на 31 и добавляем ASCII код символа
    }
    return hash;
}
