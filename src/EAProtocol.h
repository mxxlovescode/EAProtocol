// EAprotocol.h
#ifndef EA_PROTOCOL_H
#define EA_PROTOCOL_H

// комманда #SENDTOSERV;12;13;1422455;

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoLog.h> // Подключение ArduinoLog для логирования
#include <mString.h>

#define COMMAND_MARKER "#"
#define COMMAND_DIVIDER ';'

#define MBUFFER_SIZE 256 // Определяем величину буфера обмена для гайверовской библиотеки

#define MAX_NUMBER_OF_COMMAND 5 // Определяем количество комманд

class EAprotocol {
public:

    // Конструктор и деструктор
    EAprotocol(HardwareSerial &serialPort, char endOfMessage = '\n', unsigned long timeout = 1000);

    void begin();

    ~EAprotocol();

    void tick();     // Основной цикл обработки

    struct Command {
        uint32_t command_name_hash;     // ХЭШ с именем команды
        void (*handler)(const char*);   // Указатель на функцию-обработчик
    };

    void registerCommand(const char* commandName, void (*handler)(const char*));

    void executeCommand(const char* command);

    void sendCommand(const String &command, const String &data);

    void readDataToBuffer();

    void processMessage();

    char *getBuff();

private:

    // Членские переменные
    HardwareSerial &_serial;
    char _endOfMessage;
    unsigned long _timeout;

    void _handleLog();                                                              // Стандарнатная обработка если пришли Логи

    uint32_t hashString(const char *str);

    Command commands[MAX_NUMBER_OF_COMMAND];
    int commandCount;
    void (*commandHandlers[MAX_NUMBER_OF_COMMAND])(const char* args);       // Массив указателей на функции-обработчики
    

    mString<MBUFFER_SIZE> _mBuffer; // Буфер
};

#endif // EA_PROTOCOL_H

