#ifndef EA_PROTOCOL_H
#define EA_PROTOCOL_H

// Комманда #SENDTOSERV;12;13;1422455;

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoLog.h> // Подключение ArduinoLog для логирования
#include <mString.h>

#define COMMAND_MARKER "#"          // Маркер начала команды
#define COMMAND_DIVIDER ';'          // Разделитель внутри команды

#define MBUFFER_SIZE 256             // Размер буфера для обработки сообщений
#define MAX_NUMBER_OF_COMMAND 5      // Максимальное количество поддерживаемых команд

class EAprotocol {
public:

    // Конструктор и деструктор
    // serialPort: аппаратный последовательный порт
    // endOfMessage: символ завершения сообщения (по умолчанию '\n')
    // timeout: время ожидания завершения команды (по умолчанию 1000 мс)
    EAprotocol(HardwareSerial &serialPort, char endOfMessage = '\n', unsigned long timeout = 1000);

    void begin();  // Инициализация порта и буфера

    ~EAprotocol(); // Деструктор

    void tick();   // Основной метод обработки данных

    // Структура для хранения команды и её обработчика
    struct Command {
        uint32_t command_name_hash;     // Хеш имени команды
        void (*handler)(const char*);   // Указатель на функцию-обработчик
    };

    // Регистрация команды
    // commandName: имя команды
    // handler: обработчик команды
    void registerCommand(const char* commandName, void (*handler)(const char*));

    // Выполнение команды
    // command: строка с именем команды
    void executeCommand(const char* command);

    // Отправка команды с данными
    // command: имя команды
    // data: данные для передачи
    void sendCommand(const String &command, const String &data);

    // Чтение данных из последовательного порта во внутренний буфер
    void readDataToBuffer();

    // Обработка сообщения из буфера
    void processMessage();

    // Возвращает содержимое буфера
    char *getBuff();

private:

    // Членские переменные
    HardwareSerial &_serial;            // Ссылка на аппаратный последовательный порт
    char _endOfMessage;                 // Символ окончания сообщения
    unsigned long _timeout;             // Таймаут ожидания данных

    void _handleLog();                  // Логирование сообщений, не являющихся командами

    uint32_t hashString(const char *str); // Функция для хеширования строк

    Command commands[MAX_NUMBER_OF_COMMAND]; // Массив зарегистрированных команд
    int commandCount;                      // Текущее количество зарегистрированных команд

    mString<MBUFFER_SIZE> _mBuffer;       // Внутренний буфер для приема данных
};

#endif // EA_PROTOCOL_H
