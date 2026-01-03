#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttTopicFilter>
#include <QTimer>

class MqttClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttClient(const QString& host, quint16 port, const QString& receiveTopic, const QString& sendTopic, QObject* parent = nullptr);

    void connectToBroker();
    qint32 publish(const QString& topic, const QString& payload);
    void disconnectFromBroker();
    QMqttClient::ClientState getState();

    // ΠΡΟΣΘΗΚΕΣ για να επιτρέπεται ο καθαρισμός από την GUI
    void disconnectFromHost();
    void unsubscribe(const QMqttTopicFilter& filter);

signals:
    void on_mqtt_message(const QString& data);
    void throttleControlNeeded(const QString& payload);

public slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QByteArray& message, const QMqttTopicName& topic);

private:
    QMqttClient* m_client;
    QString m_host;
    quint16 m_port;
    QString m_receiveTopic;
    QString m_sendTopic;
};

#endif // MQTTCLIENT_H