#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <MQTT.h>
#include <map>

typedef void (*aws_iot_callback)(String& payload);

class MQTT_Manager : public MQTTClient
{
public:
    MQTT_Manager(String mqtt_name, String mqtt_host, int mqtt_port, const char* ca_cert_ptr, const char* client_cert_ptr, const char* private_key_ptr)
    {
        cert       = new BearSSL::X509List(ca_cert_ptr);
        client_crt = new BearSSL::X509List(client_cert_ptr);
        key        = new BearSSL::PrivateKey(private_key_ptr);

        mqttName = mqtt_name;
        mqttHost = mqtt_host;
        mqttPort = mqtt_port;
    }

    ~MQTT_Manager()
    {
        delete(cert);
        delete(client_crt);
        delete(key);
    }

    void begin(WiFiClientSecure& client, MQTTClientCallbackSimple callback)
    {
        client.setTrustAnchors(cert);
        client.setClientRSACert(client_crt, key);

        MQTTClient::begin(mqttHost.c_str(), mqttPort, client);
        MQTTClient::onMessage(callback);
    }

    bool on(String channel, aws_iot_callback function)
    {
        subscriptionList[channel] = function;
        return MQTTClient::subscribe(channel);
    }

    bool connect()
    {
        if (MQTTClient::connect(mqttName.c_str()))
        {
            for (auto it = subscriptionList.begin(); it != subscriptionList.end(); it++)
            {
                if (!MQTTClient::subscribe(it->first))
                {
                    return false;
                }
            }
        }
        else
        {
            return false;
        }
        return true;
    }

    void msgHandler(String &topic, String &payload)
    {
        subscriptionList[topic](payload);
    }

private:
    String mqttName;
    String mqttHost;
    int mqttPort;

    BearSSL::X509List* cert;
    BearSSL::X509List* client_crt;
    BearSSL::PrivateKey* key;

    std::map<String, aws_iot_callback> subscriptionList;

};


#endif
