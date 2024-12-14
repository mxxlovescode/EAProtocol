// EAprotocol.h
#ifndef EA_PROTOCOL_H
#define EA_PROTOCOL_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoLog.h> // Подключение ArduinoLog для логирования
#include <mString.h>

#define MS_EXTERNAL // mstring external buffer enabled

#define COMMAND_MARKER '#'

#define MAX_COMMANDS 10
#define MAX_COMMAND_LENGTH 20

#define MBUFFER_SIZE 256 // Определяем величину буфера обмена для гайверовской библиотеки

// Массив для хранения команд
char commandNames[MAX_COMMANDS][MAX_COMMAND_LENGTH];
int commandCount = 0;


class EAprotocol {
public:
    // Конструктор и деструктор
    EAprotocol(HardwareSerial &serialPort, char endOfMessage = '\n', unsigned long timeout = 1000, size_t bufferSize = 256);

    ~EAprotocol();

    void tick();     // Основной цикл обработки
    void registerCommand(const char* commandName, void (*handler)(const char*));

    void sendCommand(const String &command, const String &data);

    void readDataToBuffer();
    void processMessage();
private:

    // Членские переменные
    HardwareSerial &_serial;
    char _endOfMessage;
    unsigned long _timeout;

    void _handleLog();                                                              // Стандарнатная обработка если пришли Логи
    
    // Массив указателей на функции-обработчики
    void (*commandHandlers[MAX_COMMANDS])(const char* args);

    size_t _bufferSize;                                                             // Размер буфера
    char *_buffer;
                                                                      // Буфер для хранения данных
    mString<MBUFFER_SIZE> _mBuffer;                                                 // Буфер
    size_t _currentBufferLength = 0;                                                // Добавлено для отслеживания текущей длины буфера
};

#endif // EA_PROTOCOL_H

