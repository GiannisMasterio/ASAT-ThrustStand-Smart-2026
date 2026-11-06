#include "mqttclient.h"
#include <QDebug>
#include <QtMqtt/QMqttTopicName> // Χρειάζεται για το publish

MqttClient::MqttClient(const QString& host, quint16 port, const QString& receiveTopic, const QString& sendTopic, QObject* parent)
    : QObject(parent),
    m_host(host),
    m_port(port),
    m_receiveTopic(receiveTopic),
    m_sendTopic(sendTopic)
{
    m_client = new QMqttClient(this);
    m_client->setHostname(m_host);
    m_client->setPort(m_port);

    connect(m_client, &QMqttClient::connected, this, &MqttClient::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttClient::onDisconnected);
    connect(m_client, &QMqttClient::messageReceived, this, &MqttClient::onMessageReceived);
}

void MqttClient::connectToBroker()
{
    if (m_client->state() != QMqttClient::Connected) {
        m_client->connectToHost();
        qDebug() << "Attempting to connect to MQTT broker...";
    }
}

void MqttClient::disconnectFromBroker()
{
    if (m_client->state() != QMqttClient::Disconnected) {
        m_client->disconnectFromHost();
        qDebug() << "Disconnected from MQTT broker.";
    }
}

qint32 MqttClient::publish(const QString& topic, const QString& payload)
{
    if (m_client->state() == QMqttClient::Connected) {
        
        qint32 id = m_client->publish(QMqttTopicName(topic), payload.toUtf8(), 1);

        if (id != -1) {
            qDebug() << "MQTT PUBLISHED: Topic=" << topic << ", Payload=" << payload << ", ID=" << id;
        }
        else {
            qWarning() << "ERROR publishing MQTT message for topic:" << topic;
        }
        return id;
    }
    else {
        qWarning() << "MQTT Client not connected. Message not sent to topic:" << topic;
        return -1;
    }
}

void MqttClient::onConnected()
{
    qDebug() << "Connected to MQTT Broker!";
    
    m_client->subscribe(QMqttTopicFilter(m_receiveTopic), 1);
    m_client->subscribe(QMqttTopicFilter(m_sendTopic), 1);
}

void MqttClient::onDisconnected()
{
    qDebug() << "Disconnected from MQTT Broker.";
}

QMqttClient::ClientState MqttClient::getState() {
    return m_client->state();
}

void MqttClient::disconnectFromHost() {

    if (m_client && m_client->state() != QMqttClient::Disconnected) {
        m_client->disconnectFromHost();
    }
}

void MqttClient::unsubscribe(const QMqttTopicFilter& filter) {
    if (m_client) {
        m_client->unsubscribe(filter);
    }
}

void MqttClient::onMessageReceived(const QByteArray& message, const QMqttTopicName& topic)
{
    QString data = QString::fromUtf8(message);
    if (topic.name() == m_receiveTopic) {
        emit on_mqtt_message(data);
    }

}