// EAprotocol.h
#ifndef EA_PROTOCOL_H
#define EA_PROTOCOL_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoLog.h> // Подключение ArduinoLog для логирования

// Константы протокола
#define EA_PROTOCOL_MAX_MESSAGE_LENGTH 256
#define EA_PROTOCOL_SEND_BUFFER_SIZE 64
#define EA_PROTOCOL_SEND_QUEUE_SIZE 10
#define EA_PROTOCOL_LARGE_DATA_THRESHOLD 128
#define MAX_COMMANDS 20

// Состояния протокола
enum ProtoState {
    PROTO_STATE_HANDSHAKE,
    PROTO_STATE_NORMAL,
    PROTO_STATE_SENDING_LARGE_DATA,
    PROTO_STATE_RECEIVING_LARGE_DATA
};

// Структура для хранения команд
struct Command {
    const char* name;
    void (*handler)(class EAprotocol* proto, const char* args);
};

// Структура для очереди отправки
struct SendPacket {
    char data[EA_PROTOCOL_SEND_BUFFER_SIZE];
    size_t length;
};

class EAprotocol {
public:
    // Конструктор и деструктор
    EAprotocol(HardwareSerial &serialPort, char endOfMessage = '\n', unsigned long timeout = 1000);
    ~EAprotocol();

    // Инициализация
    void begin(long baudRate);

    // Основной цикл обработки
    void tick();

    // Настройки протокола
    void setEndOfMessage(char eom);
    void setTimeout(unsigned long timeout);
    void setMessageCallback(void (*callback)(const char*));
    void setErrorCallback(void (*callback)(const char*));

    // Отправка данных
    bool send(const uint8_t* data, size_t size);
    bool send(const char* message);


    // Регистрация команды
    void registerCommand(const char* commandName, void(*handler)(EAprotocol* proto, const char* args));

private:
    // Методы отправки различных сообщений
    void sendHello();
    void sendHelloAck();
    void sendReadyToReceive();
    void sendDataAck();
    void sendDataNack();
    void sendReady();
    bool sendMessage(const char* message);
    bool enqueueSendPacket(const char* packet, size_t length);
    void dequeueSendPacket();
    void processOutgoingData();
    void processIncomingData();
    void parseAndDispatchCommand(const char* message);

    // Методы для работы с рукопожатием
    void startHandshake();

    // Обработчики команд
    static void handleHelloCmd(EAprotocol* proto, const char* args);
    static void handleHelloAckCmd(EAprotocol* proto, const char* args);
    static void handleSendLargeDataCmd(EAprotocol* proto, const char* args);
    static void handleReadyToReceiveCmd(EAprotocol* proto, const char* args);
    static void handleDataAckCmd(EAprotocol* proto, const char* args);
    static void handleDataNackCmd(EAprotocol* proto, const char* args);
    static void handleReadyCmd(EAprotocol* proto, const char* args);

    // Метод отправки больших данных
    bool sendLargeData(const uint8_t* data, size_t size);

    // Метод освобождения ресурсов при необходимости
    void cleanupLargeDataBuffer();

    // Членские переменные
    HardwareSerial &_serial;
    char _endOfMessage;
    unsigned long _timeout;
    unsigned long _lastTime;
    bool _receiving;
    size_t _incomingLength;
    char _incomingData[EA_PROTOCOL_MAX_MESSAGE_LENGTH];
    void (*_messageCallback)(const char*);
    void (*_errorCallback)(const char*);

    // Очередь отправки
    SendPacket _sendQueue[EA_PROTOCOL_SEND_QUEUE_SIZE];
    size_t _sendQueueHead;
    size_t _sendQueueTail;
    size_t _sendQueueCount;
    bool _isSending;
    size_t _currentSendPos;

    // Состояния протокола
    ProtoState _state;

    // Рукопожатие
    unsigned int _commandCount;
    unsigned long _handshakeStartTime;
    unsigned long _handshakeInterval;
    bool _handshakeInitiated;

    // Обработка больших данных
    uint8_t* _largeDataBuffer;
    size_t _largeDataSize;
    size_t _largeDataReceived;
    size_t _largeDataSent;
    bool _awaitingDataAck;
    void (*_largeDataReceiveCallback)(const uint8_t*, size_t);
    bool _receiverReady;

    // Команды
    Command _commands[MAX_COMMANDS];
};

#endif // EA_PROTOCOL_H
