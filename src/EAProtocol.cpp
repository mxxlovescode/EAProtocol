// EAprotocol.cpp
#include "EAprotocol.h"
#include <string.h> // Для работы со строками
#include <stdio.h>  // Для snprintf и sscanf
#include <new>      // Для std::nothrow

// Деструктор для освобождения выделенной памяти
EAprotocol::~EAprotocol() {
    Log.verbose(F("Деструктор вызван: освобождаем большой буфер данных.\r\n"));
    cleanupLargeDataBuffer();
}

// Конструктор
EAprotocol::EAprotocol(HardwareSerial &serialPort, char endOfMessage, unsigned long timeout)
  : _serial(serialPort), _endOfMessage(endOfMessage), _timeout(timeout),
    _lastTime(0), _receiving(false), _incomingLength(0),
    _messageCallback(nullptr), _errorCallback(nullptr),
    _sendQueueHead(0), _sendQueueTail(0), _sendQueueCount(0),
    _isSending(false), _currentSendPos(0),
    _state(PROTO_STATE_HANDSHAKE),
    _commandCount(0),
    _handshakeStartTime(0),
    _handshakeInterval(1000),
    _handshakeInitiated(false),
    _largeDataBuffer(nullptr),
    _largeDataSize(0),
    _largeDataReceived(0),
    _largeDataSent(0),
    _awaitingDataAck(false),
    _largeDataReceiveCallback(nullptr),
    _receiverReady(false)
{
    memset(_incomingData, 0, EA_PROTOCOL_MAX_MESSAGE_LENGTH);
    memset(_sendQueue, 0, sizeof(_sendQueue));

    Log.notice(F("Создан экземпляр EAprotocol с EOM='%c' и таймаутом=%lu мс\r\n"), _endOfMessage, _timeout);
    
    // Регистрация базовых команд удалена, так как они обрабатываются отдельно
}

// Метод освобождения большого буфера данных
void EAprotocol::cleanupLargeDataBuffer() {
    if (_largeDataBuffer) {
        Log.verbose(F("Освобождаем большой буфер данных размером: %lu\r\n"), _largeDataSize);
        delete[] _largeDataBuffer;
        _largeDataBuffer = nullptr;
        _largeDataSize = 0;
        _largeDataReceived = 0;
        _largeDataSent = 0;
    } else {
        Log.trace(F("Нет большого буфера данных для освобождения.\r\n"));
    }
}

// Метод инициализации
void EAprotocol::begin(long baudRate) {
    _serial.begin(baudRate);
    Log.verbose(F("EAprotocol инициализирован с baud rate: %lu\r\n"), baudRate);
    startHandshake(); // Инициация рукопожатия при старте
}

// Основной цикл обработки
void EAprotocol::tick() {
    // Обработка входящих данных
    while (_serial.available() > 0) {
        char c = (char)_serial.read();
        Log.trace(F("Получен байт: '%c' (0x%02X)\r\n"), c, (uint8_t)c);

        if (_incomingLength < (EA_PROTOCOL_MAX_MESSAGE_LENGTH - 1)) {
            _incomingData[_incomingLength++] = c;
            _incomingData[_incomingLength] = '\0';
        }
        else {
            // Входящий буфер переполнен
            Log.error(F("Переполнение входящего буфера. Сбрасываем буфер.\r\n"));
            if (_errorCallback != nullptr) {
                _errorCallback("Incoming buffer overflow");
            } else {
                _serial.println(F("Error: Incoming buffer overflow"));
            }
            // Сбрасываем буфер
            _incomingLength = 0;
            _receiving = false;
            continue;
        }

        _lastTime = millis();
        _receiving = true;

        // Проверяем конец сообщения
        if (c == _endOfMessage) {
            Log.notice(F("Получен конец сообщения. Обрабатываем входящие данные.\r\n"));
            processIncomingData();
            _incomingLength = 0;
            _receiving = false;
        }
    }

    // Проверяем таймаут, если данные принимаются, но конец не получен
    if (_receiving && (millis() - _lastTime > _timeout)) {
        Log.warning(F("Достигнут таймаут приема. Обрабатываем неполные данные.\r\n"));
        processIncomingData();
        _incomingLength = 0;
        _receiving = false;
    }

    // Обработка исходящих данных
    processOutgoingData();

    // Если мы все еще в режиме рукопожатия и инициатива была начата, посылаем HELLO периодически
    if (_state == PROTO_STATE_HANDSHAKE && _handshakeInitiated) {
        if (millis() - _handshakeStartTime > _handshakeInterval) {
            Log.notice(F("Рукопожатие в процессе. Отправляем HELLO.\r\n"));
            sendHello();
            _handshakeStartTime = millis();
        }
    }
}

// Установка символа окончания сообщения
void EAprotocol::setEndOfMessage(char eom) {
    Log.notice(F("Устанавливаем символ окончания сообщения: '%c' (0x%02X)\r\n"), eom, (uint8_t)eom);
    _endOfMessage = eom;
}

// Установка таймаута
void EAprotocol::setTimeout(unsigned long timeout) {
    Log.notice(F("Устанавливаем таймаут: %lu мс\r\n"), timeout);
    _timeout = timeout;
}

// Установка колбэка для обработки полученных сообщений
void EAprotocol::setMessageCallback(void (*callback)(const char*)) {
    _messageCallback = callback;
    Log.verbose(F("Установлен колбэк для обработки сообщений.\r\n"));
}

// Установка колбэка для обработки ошибок
void EAprotocol::setErrorCallback(void (*callback)(const char*)) {
    _errorCallback = callback;
    Log.verbose(F("Установлен колбэк для обработки ошибок.\r\n"));
}

// Метод отправки данных
bool EAprotocol::send(const uint8_t* data, size_t size) {
    Log.trace(F("Отправка данных размером: %lu байт\r\n"), size);
    if (size <= EA_PROTOCOL_LARGE_DATA_THRESHOLD) {
        // Отправляем как обычное сообщение
        char message[EA_PROTOCOL_MAX_MESSAGE_LENGTH];
        if (size + 2 > EA_PROTOCOL_MAX_MESSAGE_LENGTH) { // +1 для символа окончания и +1 для '\0'
            Log.error(F("Размер сообщения превышает максимально допустимый: %lu байт\r\n"), size);
            if (_errorCallback) {
                _errorCallback("Message size exceeds maximum allowed size");
            }
            return false;
        }

        memcpy(message, data, size);
        message[size] = _endOfMessage;
        message[size + 1] = '\0';

        // Добавляем пакет в очередь только если получатель готов
        if (_receiverReady) {
            if (enqueueSendPacket(message, size + 1)) {
                Log.verbose(F("Пакет добавлен в очередь отправки. Размер: %lu байт\r\n"), size + 1);
                _receiverReady = false; // Сбрасываем флаг готовности
                return true;
            } else {
                Log.error(F("Переполнение очереди отправки.\r\n"));
                if (_errorCallback) {
                    _errorCallback("Send queue overflow");
                }
                return false;
            }
        } else {
            // Получатель не готов, уведомляем об этом
            Log.warning(F("Получатель не готов к приему данных.\r\n"));
            if (_errorCallback) {
                _errorCallback("Receiver not ready");
            }
            return false;
        }
    }
    else {
        // Отправляем как большие данные
        return sendLargeData(data, size);
    }
}

bool EAprotocol::send(const char* message) {
    // Проверяем валидность указателя
    if (!message) {
        Log.error(F("Попытка отправить нулевое сообщение.\r\n"));
        if (_errorCallback) {
            _errorCallback("Message is null");
        }
        return false;
    }

    // Определяем длину строки с ограничением по максимальному размеру
    size_t size = 0;
    while (size < EA_PROTOCOL_MAX_MESSAGE_LENGTH && message[size] != '\0') {
        size++;
    }

    // Проверяем корректность размера сообщения
    if (size == 0) {
        Log.error(F("Сообщение пустое.\r\n"));
        if (_errorCallback) {
            _errorCallback("Message is empty");
        }
        return false;
    }

    if (size >= EA_PROTOCOL_MAX_MESSAGE_LENGTH) {
        Log.error(F("Размер сообщения превышает допустимый лимит: %zu байт.\r\n"), size);
        if (_errorCallback) {
            _errorCallback("Message size exceeds maximum allowed size");
        }
        return false;
    }

    Log.trace(F("Отправка строки: '%s' размером: %zu байт\r\n"), message, size);

    // Вызываем существующий метод для отправки данных
    bool result = send(reinterpret_cast<const uint8_t*>(message), size);

    if (!result) {
        Log.error(F("Ошибка отправки сообщения: '%s'.\r\n"), message);
        if (_errorCallback) {
            _errorCallback("Failed to send message");
        }
    } else {
        Log.trace(F("Сообщение успешно отправлено: '%s'.\r\n"), message);
    }

    return result;
}

// Метод отправки больших данных
bool EAprotocol::sendLargeData(const uint8_t* data, size_t size) {
    Log.trace(F("Отправка больших данных размером: %lu байт\r\n"), size);
    if (_state != PROTO_STATE_NORMAL) {
        Log.warning(F("Невозможно отправить большие данные в текущем состоянии: %d\r\n"), _state);
        if (_errorCallback) {
            _errorCallback("Cannot send large data in current state");
        }
        return false;
    }

    // Сохраняем данные для отправки
    _largeDataBuffer = new(std::nothrow) uint8_t[size];
    if (_largeDataBuffer == nullptr) {
        Log.error(F("Не удалось выделить память для больших данных размером: %lu байт\r\n"), size);
        if (_errorCallback) {
            _errorCallback("Failed to allocate memory for large data");
        }
        return false;
    }

    memcpy(_largeDataBuffer, data, size);
    _largeDataSize = size;
    _largeDataSent = 0;
    _state = PROTO_STATE_SENDING_LARGE_DATA;
    Log.notice(F("Переход в состояние отправки больших данных.\r\n"));

    // Отправляем команду о начале передачи больших данных
    char cmd[64];
    int n = snprintf(cmd, sizeof(cmd), "SEND_LARGE_DATA size=%u", static_cast<unsigned int>(size));
    if (n < 0 || static_cast<size_t>(n) >= sizeof(cmd)) {
        Log.error(F("Не удалось сформатировать команду SEND_LARGE_DATA.\r\n"));
        if (_errorCallback) {
            _errorCallback("Failed to format SEND_LARGE_DATA command");
        }
        cleanupLargeDataBuffer();
        _state = PROTO_STATE_NORMAL;
        return false;
    }

    Log.trace(F("Отправка команды SEND_LARGE_DATA: '%s'\r\n"), cmd);
    if (!sendMessage(cmd)) {
        Log.error(F("Не удалось добавить команду SEND_LARGE_DATA в очередь отправки.\r\n"));
        if (_errorCallback) {
            _errorCallback("Failed to enqueue SEND_LARGE_DATA command");
        }
        cleanupLargeDataBuffer();
        _state = PROTO_STATE_NORMAL;
        return false;
    }

    return true;
}

// Метод отправки сообщения
bool EAprotocol::sendMessage(const char* message) {
    if (message == nullptr) {
        Log.error(F("Входное сообщение имеет нулевой указатель.\r\n"));
        if (_errorCallback) {
            _errorCallback("Null message pointer");
        }
        return false;
    }

    // Проверка корректности строки
    const char* endPtr = static_cast<const char*>(memchr(message, '\0', EA_PROTOCOL_SEND_BUFFER_SIZE));
    if (endPtr == nullptr) {
        Log.error(F("Сообщение не имеет корректного завершения. Возможно, превышен допустимый размер.\r\n"));
        if (_errorCallback) {
            _errorCallback("Message not null-terminated");
        }
        return false;
    }

    size_t messageLen = endPtr - message; // Вычисляем длину безопасно
    if (messageLen + 2 > EA_PROTOCOL_SEND_BUFFER_SIZE) { // +1 для символа окончания и +1 для '\0'
        Log.error(F("Сообщение слишком длинное для отправки: %zu байт\r\n"), messageLen);
        if (_errorCallback) {
            _errorCallback("Message too long to send");
        }
        return false;
    }

    Log.trace(F("Отправка сообщения: '%s' размером: %zu байт\r\n"), message, messageLen);

    // Сформировать пакет с символом окончания
    char packet[EA_PROTOCOL_SEND_BUFFER_SIZE];
    strncpy(packet, message, EA_PROTOCOL_SEND_BUFFER_SIZE - 2); // -2 для окончания и '\0'
    packet[messageLen] = _endOfMessage; // Добавляем символ окончания
    packet[messageLen + 1] = '\0';      // Завершаем строку

    Log.trace(F("Сформированный пакет для отправки: '%s'\r\n"), packet);

    // Попытка добавить пакет в очередь
    if (!enqueueSendPacket(packet, messageLen + 1)) { // messageLen + 1 включает _endOfMessage
        Log.error(F("Не удалось добавить пакет в очередь отправки.\r\n"));
        if (_errorCallback) {
            _errorCallback("Failed to enqueue packet");
        }
        return false;
    }

    Log.verbose(F("Пакет успешно добавлен в очередь отправки.\r\n"));
    return true;
}

// Метод добавления пакета в очередь отправки
bool EAprotocol::enqueueSendPacket(const char* packet, size_t length) {
    Log.trace(F("Добавление пакета в очередь отправки. Размер: %lu байт\r\n"), length);
    if (length >= EA_PROTOCOL_SEND_BUFFER_SIZE) {
        // Пакет слишком длинный для очереди
        Log.error(F("Пакет слишком длинный для очереди отправки: %lu байт\r\n"), length);
        return false;
    }

    if (_sendQueueCount >= EA_PROTOCOL_SEND_QUEUE_SIZE) {
        // Очередь переполнена
        Log.error(F("Очередь отправки переполнена.\r\n"));
        return false;
    }

    // Копируем данные в очередь
    memcpy(_sendQueue[_sendQueueTail].data, packet, length);
    _sendQueue[_sendQueueTail].data[length] = '\0'; // Убедимся, что пакет завершён нулём
    _sendQueue[_sendQueueTail].length = length;

    Log.verbose(F("Пакет добавлен в очередь отправки на позиции: %d\r\n"), _sendQueueTail);

    // Обновляем хвост и счётчик
    _sendQueueTail = (_sendQueueTail + 1) % EA_PROTOCOL_SEND_QUEUE_SIZE;
    _sendQueueCount++;

    return true;
}

// Метод удаления пакета из очереди отправки
void EAprotocol::dequeueSendPacket() {
    if (_sendQueueCount <= 0) {
        Log.trace(F("Очередь отправки пуста. Нечего удалять.\r\n"));
        return;
    }
    Log.verbose(F("Удаление пакета из очереди отправки с позиции: %d\r\n"), _sendQueueHead);
    memset(_sendQueue[_sendQueueHead].data, 0, EA_PROTOCOL_SEND_BUFFER_SIZE);
    _sendQueue[_sendQueueHead].length = 0;
    _sendQueueHead = (_sendQueueHead + 1) % EA_PROTOCOL_SEND_QUEUE_SIZE;
    _sendQueueCount--;
}

// Метод обработки исходящих данных
void EAprotocol::processOutgoingData() {
    if (!_isSending && _sendQueueCount > 0) {
        _isSending = true;
        _currentSendPos = 0;
        Log.trace(F("Начало отправки нового пакета из очереди.\r\n"));
    }

    if (_isSending && _sendQueueCount > 0) {
        SendPacket &currentPacket = _sendQueue[_sendQueueHead];
        size_t bytesToSend = currentPacket.length - _currentSendPos;

        size_t bytesAvailable = _serial.availableForWrite();
        Log.trace(F("Доступно для записи байт: %lu\r\n"), bytesAvailable);
        if (bytesAvailable == 0) {
            return;
        }

        size_t bytesToWrite = (bytesToSend < bytesAvailable) ? bytesToSend : bytesAvailable;
        _serial.write(reinterpret_cast<const uint8_t*>(currentPacket.data) + _currentSendPos, bytesToWrite);
        Log.verbose(F("Отправлено %lu байт из текущего пакета.\r\n"), bytesToWrite);

        _currentSendPos += bytesToWrite;

        if (_currentSendPos >= currentPacket.length) {
            // Пакет полностью отправлен
            Log.trace(F("Пакет полностью отправлен. Удаляем из очереди.\r\n"));
            dequeueSendPacket();
            _isSending = false;
        }
    }
}

// Метод обработки входящих данных
void EAprotocol::processIncomingData() {
    Log.notice(F("Обработка входящих данных: '%s'\r\n"), _incomingData);
    // Имеется полное сообщение в _incomingData
    parseAndDispatchCommand(_incomingData);
}

// Парсинг и диспетчеризация команды или сообщения
void EAprotocol::parseAndDispatchCommand(const char* message) {
    Log.trace(F("Парсинг и диспетчеризация команды или сообщения.\r\n"));
    char buf[EA_PROTOCOL_MAX_MESSAGE_LENGTH];
    strncpy(buf, message, EA_PROTOCOL_MAX_MESSAGE_LENGTH - 1);
    buf[EA_PROTOCOL_MAX_MESSAGE_LENGTH - 1] = '\0';

    char* cmd = strtok(buf, " \t\r\n");
    if (!cmd) {
        // Пустое сообщение
        Log.warning(F("Получено пустое сообщение.\r\n"));
        if (_messageCallback) {
            _messageCallback(message);
        }
        return;
    }

    char* args = strtok(NULL, "\n");
    if (!args) args = const_cast<char*>("");

    Log.trace(F("Распознанная команда: '%s' с аргументами: '%s'\r\n"), cmd, args);

    // Ищем команду
    for (size_t i = 0; i < _commandCount; i++) {
        if (strcmp(_commands[i].name, cmd) == 0 && _commands[i].handler) {
            Log.notice(F("Найдена зарегистрированная команда: '%s'. Вызываем обработчик.\r\n"), cmd);
            // Используем зарегистрированные обработчики команд
            _commands[i].handler(this, args);
            return;
        }
    }

    // Обрабатываем базовые команды отдельно
    if (strcmp(cmd, "HELLO") == 0) {
        Log.notice(F("Обнаружена команда HELLO.\r\n"));
        handleHelloCmd(this, args);
        return;
    }
    if (strcmp(cmd, "HELLO_ACK") == 0) {
        Log.notice(F("Обнаружена команда HELLO_ACK.\r\n"));
        handleHelloAckCmd(this, args);
        return;
    }
    if (strcmp(cmd, "SEND_LARGE_DATA") == 0) {
        Log.notice(F("Обнаружена команда SEND_LARGE_DATA.\r\n"));
        handleSendLargeDataCmd(this, args);
        return;
    }
    if (strcmp(cmd, "READY_TO_RECEIVE") == 0) {
        Log.notice(F("Обнаружена команда READY_TO_RECEIVE.\r\n"));
        handleReadyToReceiveCmd(this, args);
        return;
    }
    if (strcmp(cmd, "DATA_ACK") == 0) {
        Log.notice(F("Обнаружена команда DATA_ACK.\r\n"));
        handleDataAckCmd(this, args);
        return;
    }
    if (strcmp(cmd, "DATA_NACK") == 0) {
        Log.notice(F("Обнаружена команда DATA_NACK.\r\n"));
        handleDataNackCmd(this, args);
        return;
    }
    if (strcmp(cmd, "READY") == 0) {
        Log.notice(F("Обнаружена команда READY.\r\n"));
        handleReadyCmd(this, args);
        return;
    }

    // Если команда не найдена - это обычное сообщение
    Log.warning(F("Команда не распознана. Обработка как обычного сообщения.\r\n"));
    if (_messageCallback) {
        _messageCallback(message);
    } else {
        // По умолчанию выводим полученные данные обратно (для отладки)
        Log.notice(F("Получено сообщение: '%s'\r\n"), message);
        _serial.print(F("Received: "));
        _serial.println(message);
    }
}

// Метод регистрации команды
void EAprotocol::registerCommand(const char* commandName, void(*handler)(EAprotocol* proto, const char* args)) {
    if (_commandCount < MAX_COMMANDS) {
        _commands[_commandCount].name = commandName;
        _commands[_commandCount].handler = handler;
        _commandCount++;
        Log.notice(F("Зарегистрирована команда: '%s'\r\n"), commandName);
    } else {
        Log.error(F("Превышено максимальное количество зарегистрированных команд.\r\n"));
        if (_errorCallback) {
            _errorCallback("Too many commands registered");
        } else {
            _serial.println(F("Error: Too many commands registered"));
        }
    }
}

// Метод запуска рукопожатия
void EAprotocol::startHandshake() {
    Log.notice(F("Запуск рукопожатия.\r\n"));
    _state = PROTO_STATE_HANDSHAKE;
    _handshakeInitiated = true;
    _handshakeStartTime = millis();
    sendHello();
}

// Обработчик команды HELLO
void EAprotocol::handleHelloCmd(EAprotocol* proto, const char* args) {
    Log.notice(F("Обработка команды HELLO с аргументами: '%s'\r\n"), args);
    // Получили HELLO от другого устройства
    // Можно распарсить args, напр. version=1.0 buffers=256
    // В ответ шлем HELLO_ACK
    proto->sendHelloAck();
    Log.verbose(F("Отправлен HELLO_ACK в ответ на HELLO.\r\n"));

    // Если мы были в режиме рукопожатия, переходим в нормальный режим
    if (proto->_state == PROTO_STATE_HANDSHAKE) {
        proto->_state = PROTO_STATE_NORMAL;
        proto->_handshakeInitiated = false;
        // Отправляем READY, чтобы уведомить отправителя о готовности
        proto->sendReady();
        proto->_receiverReady = true;
        Log.notice(F("Переход в нормальный режим после получения HELLO.\r\n"));
    }
}

// Обработчик команды HELLO_ACK
void EAprotocol::handleHelloAckCmd(EAprotocol* proto, const char* args) {
    Log.notice(F("Обработка команды HELLO_ACK с аргументами: '%s'\r\n"), args);
    // Получили HELLO_ACK
    // Аналогично можно распарсить args.
    // Переходим в нормальный режим
    proto->_state = PROTO_STATE_NORMAL;
    proto->_handshakeInitiated = false;
    // Отправляем READY, чтобы уведомить отправителя о готовности
    proto->sendReady();
    proto->_receiverReady = true;
    Log.notice(F("Переход в нормальный режим после получения HELLO_ACK.\r\n"));
}

// Обработчик команды SEND_LARGE_DATA
void EAprotocol::handleSendLargeDataCmd(EAprotocol* proto, const char* args) {
    Log.notice(F("Обработка команды SEND_LARGE_DATA с аргументами: '%s'\r\n"), args);
    // Парсим размер данных
    unsigned int size_uint = 0;
    int parsed = sscanf(args, "size=%u", &size_uint);
    if (parsed != 1) {
        Log.error(F("Не удалось распарсить размер для больших данных.\r\n"));
        if (proto->_errorCallback) {
            proto->_errorCallback("Failed to parse size for large data");
        }
        return;
    }

    size_t size = static_cast<size_t>(size_uint);
    Log.verbose(F("Запрошен прием больших данных размером: %lu байт\r\n"), size);

    if (size == 0) {
        Log.error(F("Неверный размер больших данных: 0 байт.\r\n"));
        if (proto->_errorCallback) {
            proto->_errorCallback("Invalid large data size");
        }
        return;
    }

    // Проверяем, можем ли мы выделить буфер
    proto->_largeDataBuffer = new(std::nothrow) uint8_t[size];
    if (proto->_largeDataBuffer == nullptr) {
        // Не удалось выделить память
        Log.error(F("Не удалось выделить память для больших данных размером: %lu байт\r\n"), size);
        proto->sendDataNack();
        if (proto->_errorCallback) {
            proto->_errorCallback("Failed to allocate memory for large data");
        }
        return;
    }

    proto->_largeDataSize = size;
    proto->_largeDataReceived = 0;
    proto->_state = PROTO_STATE_RECEIVING_LARGE_DATA;
    Log.notice(F("Переход в состояние приема больших данных.\r\n"));

    // Отправляем подтверждение готовности к приёму
    proto->sendReadyToReceive();
    Log.verbose(F("Отправлено подтверждение READY_TO_RECEIVE.\r\n"));
}

// Обработчик команды READY_TO_RECEIVE
void EAprotocol::handleReadyToReceiveCmd(EAprotocol* proto, const char* args) {
    Log.notice(F("Обработка команды READY_TO_RECEIVE с аргументами: '%s'\r\n"), args);
    // Готовность к приему данных
    if (proto->_state != PROTO_STATE_SENDING_LARGE_DATA) {
        Log.warning(F("Получена команда READY_TO_RECEIVE, но состояние не SENDING_LARGE_DATA.\r\n"));
        return;
    }

    // Начинаем передачу данных
    size_t bytesLeft = proto->_largeDataSize - proto->_largeDataSent;
    size_t chunkSize = (bytesLeft < (EA_PROTOCOL_SEND_BUFFER_SIZE - 1)) ? bytesLeft : (EA_PROTOCOL_SEND_BUFFER_SIZE - 1);

    if (chunkSize > 0) {
        char dataPacket[EA_PROTOCOL_SEND_BUFFER_SIZE];
        memcpy(dataPacket, proto->_largeDataBuffer + proto->_largeDataSent, chunkSize);
        dataPacket[chunkSize] = proto->_endOfMessage; // Добавляем символ окончания
        dataPacket[chunkSize + 1] = '\0'; // Завершаем строку нулём

        Log.trace(F("Отправка блока больших данных размером: %lu байт\r\n"), chunkSize);
        if (!proto->sendMessage(dataPacket)) {
            Log.error(F("Не удалось отправить пакет больших данных.\r\n"));
            if (proto->_errorCallback) {
                proto->_errorCallback("Failed to send data packet");
            }
            // Можно реализовать дополнительную логику при ошибке
        }
        proto->_largeDataSent += chunkSize;
    }

    // Если все данные отправлены, ожидаем подтверждения
    if (proto->_largeDataSent >= proto->_largeDataSize) {
        proto->_awaitingDataAck = true;
        proto->_state = PROTO_STATE_NORMAL;
        Log.notice(F("Все большие данные отправлены. Ожидаем подтверждения.\r\n"));
    }
}

// Обработчик команды DATA_ACK
void EAprotocol::handleDataAckCmd(EAprotocol* proto, const char* args) {
    Log.notice(F("Обработка команды DATA_ACK с аргументами: '%s'\r\n"), args);
    if (!proto->_awaitingDataAck) {
        Log.warning(F("Получена DATA_ACK, но не ожидалось подтверждение данных.\r\n"));
        return;
    }

    // Передача данных успешна
    proto->_state = PROTO_STATE_NORMAL;
    proto->_awaitingDataAck = false;

    // Освобождаем буфер
    proto->cleanupLargeDataBuffer();
    Log.verbose(F("Большой буфер данных освобожден после успешной передачи.\r\n"));

    // Отправляем READY, чтобы уведомить отправителя о готовности к новым данным
    proto->sendReady();
    proto->_receiverReady = true;
    Log.notice(F("Отправлено READY после успешной передачи больших данных.\r\n"));

    if (proto->_messageCallback) {
        proto->_messageCallback("Large data sent successfully");
    }
}

// Обработчик команды DATA_NACK
void EAprotocol::handleDataNackCmd(EAprotocol* proto, const char* args) {
    Log.notice(F("Обработка команды DATA_NACK с аргументами: '%s'\r\n"), args);
    if (!proto->_awaitingDataAck) {
        Log.warning(F("Получена DATA_NACK, но не ожидалось подтверждение данных.\r\n"));
        return;
    }

    // Передача данных не удалась
    proto->_state = PROTO_STATE_NORMAL;
    proto->_awaitingDataAck = false;

    // Освобождаем буфер
    proto->cleanupLargeDataBuffer();
    Log.error(F("Передача больших данных завершилась неудачно.\r\n"));

    if (proto->_errorCallback) {
        proto->_errorCallback("Large data transmission failed");
    }

    // Отправляем READY, чтобы уведомить отправителя о готовности к новым данным
    proto->sendReady();
    proto->_receiverReady = true;
    Log.notice(F("Отправлено READY после неудачной передачи больших данных.\r\n"));
}

// Обработчик команды READY
void EAprotocol::handleReadyCmd(EAprotocol* proto, const char* args) {
    Log.notice(F("Обработка команды READY с аргументами: '%s'\r\n"), args);
    // Получили сигнал готовности получателя
    proto->_receiverReady = true;
    Log.verbose(F("Получатель готов к приему данных.\r\n"));
}

// Метод отправки HELLO
void EAprotocol::sendHello() {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "HELLO version=1.0 buffers=%u", 
                     static_cast<unsigned int>(EA_PROTOCOL_MAX_MESSAGE_LENGTH));
    
    // Проверяем результат snprintf
    if (n < 0) {
        Log.error(F("Ошибка при форматировании сообщения HELLO.\r\n"));
        if (_errorCallback) {
            _errorCallback("Failed to format HELLO message");
        }
        return;
    } 
    if (static_cast<size_t>(n) >= sizeof(buf)) {
        Log.error(F("Сообщение HELLO слишком длинное и было обрезано.\r\n"));
        if (_errorCallback) {
            _errorCallback("HELLO message truncated");
        }
        return;
    }
    
    Log.trace(F("Отправка сообщения HELLO: '%s'\r\n"), buf);
    sendMessage(buf);
}

// Метод отправки HELLO_ACK
void EAprotocol::sendHelloAck() {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "HELLO_ACK version=1.0 buffers=%u", static_cast<unsigned int>(EA_PROTOCOL_MAX_MESSAGE_LENGTH));
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        Log.error(F("Не удалось сформатировать сообщение HELLO_ACK.\r\n"));
        if (_errorCallback) {
            _errorCallback("Failed to format HELLO_ACK message");
        }
        return;
    }
    Log.trace(F("Отправка сообщения HELLO_ACK: '%s'\r\n"), buf);
    sendMessage(buf);
}

// Метод отправки READY_TO_RECEIVE
void EAprotocol::sendReadyToReceive() {
    Log.trace(F("Отправка сообщения READY_TO_RECEIVE.\r\n"));
    sendMessage("READY_TO_RECEIVE");
}

// Метод отправки DATA_ACK
void EAprotocol::sendDataAck() {
    Log.trace(F("Отправка сообщения DATA_ACK.\r\n"));
    sendMessage("DATA_ACK");
}

// Метод отправки DATA_NACK
void EAprotocol::sendDataNack() {
    Log.trace(F("Отправка сообщения DATA_NACK.\r\n"));
    sendMessage("DATA_NACK");
}

// Метод отправки READY
void EAprotocol::sendReady() {
    Log.trace(F("Отправка сообщения READY.\r\n"));
    sendMessage("READY");
}
