#include <stdio.h>
#include <mosquitto.h>
#include <string>
#include <Tarts.h>
#include <TartsStrings.h>
#include <ncurses.h>
#include <jsoncpp/json/json.h>
#include <ctime>

#define GATEWAY_ID  "T5YC28"
#define SENSOR_ID   "T5YFLY"

// Tarts on BeagleBoneBlack
#ifdef BB_BLACK_ARCH
    #define GATEWAY_CHANNELS      0xFFFFFFFF // All Channels
    #define GATEWAY_UARTNUMBER    2          //<<your UART Selection>>
    #define GATEWAY_PINACTIVITY   P9_42      //<<your Activity Pin location>>
    #define GATEWAY_PINPCTS       P8_26      //<<your PCTS Pin location>>
    #define GATEWAY_PINPRTS       P8_15      //<<your PRTS Pin location>>
    #define GATEWAY_PINRESET      P8_12      //<<your Reset Pin location>>
#endif

struct mosquitto* mosquittoClient;

void SensorGatewayPersist_callback(const char* id){
    printf("TARTS-GWP[%s]\n", id);
    //Save Data about this gateway here.  This will all a user to store key values about the current gateway that could be used to resore back too after reset.
    //TartsGateway* gw = Tarts.FindGateway(id);
    //gw.getChannelMask();     //Permitted channels available during a reform. (0xFFFFFFFF -> All channels enabled, 0x000000FF -> Chan[0-7] enabled)
}


void PrintSensorSettings(const char* id) {
    TartsSensorBase* sensor = Tarts.FindSensor(id);
    printf("Sensor type=%d\n", sensor->getSensorType());
    printf("Sensor interval=%d\n", sensor->getReportInterval());
    printf("Sensor link interval=%d\n", sensor->getLinkInterval());
    printf("Sensor retry count=%d\n", sensor->getRetryCount());
    printf("Sensor recovery=%d\n", sensor->getRecovery());
}

void SensorPersist_callback(const char* id){
    printf("TARTS-SENP[%s]\n", id);
    //Save Data about this sensor here.  This will all a user to store key values about the current sensor that could be used to resore back to after reset.
    PrintSensorSettings(id);
}

void SensorGatewayMessage_callback(const char* id, int stringID){
    printf("TARTS-GWM[%s]-%d: %s\n", id, stringID, TartsGatewayStringTable[stringID]);
    if(stringID == 10) printf("ACTIVE - Channel: %d\n", Tarts.FindGateway(GATEWAY_ID)->getOperatingChannel());
}


const std::string Now() {
    time_t    now = time(0);
    struct tm tstruct;
    char      buf[80];

    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %T", &tstruct);

    return buf;
}
     

void SensorMessage_callback(SensorMessage* msg){
    bool detectedMotion = false;
    int8_t signalStrength;
    uint16_t batteryVoltage;
    std::string timestamp = Now();

    printf("TARTS-SEN[%s]: RSSI: %d, dBm, Battery Voltage: %d.%02d VDC, Data: ", msg->ID, msg->RSSI, msg->BatteryVoltage/100, msg->BatteryVoltage%100);

    if (msg->DatumCount != 1) {
        printf("Error: expected only 1 Datum point on sensor callback, but got %d.", msg->DatumCount);
        return;
    }

    detectedMotion = ! strcmp(msg->DatumList[0].Value, "1");
    batteryVoltage = msg->BatteryVoltage;
    signalStrength = msg->RSSI;


    Json::FastWriter fastWriter;
    Json::Value payload;
    payload["motion"] = detectedMotion;
    payload["timestamp"] = timestamp;
    payload["signalStrength"] = signalStrength;
    payload["batteryVoltage"] = batteryVoltage;

    std::string message = fastWriter.write(payload);
    const char *cstr = message.c_str();

    mosquitto_publish(mosquittoClient, NULL, "/greenhouse/motion" , strlen(cstr), cstr, 0, false);
}

void SensorLogException_callback(int stringID){
    printf("TARTS-EX[%u] %s\n", stringID, TartsExceptionStringTable[stringID]);
}

int SetupSensor() {
    Tarts.RegisterEvent_GatewayPersist(SensorGatewayPersist_callback);
    Tarts.RegisterEvent_GatewayMessage(SensorGatewayMessage_callback);
    Tarts.RegisterEvent_SensorPersist(SensorPersist_callback);
    Tarts.RegisterEvent_SensorMessage(SensorMessage_callback);
    Tarts.RegisterEvent_LogException(SensorLogException_callback);

    //Register Gateway
    if (!Tarts.RegisterGateway(TartsGateway::Create(GATEWAY_ID, GATEWAY_CHANNELS, GATEWAY_UARTNUMBER, GATEWAY_PINACTIVITY, GATEWAY_PINPCTS, GATEWAY_PINPRTS, GATEWAY_PINRESET))){
        printf("TARTs Gateway Registration Failed!\n");
        return 1;
    } else {
        printf("Registered sensor gateway.\n");
    }

    // TartsPassiveIR(const char* sensorID, uint16_t reportInterval, uint8_t linkInterval, uint8_t retryCount, uint8_t recovery);
    if (!Tarts.RegisterSensor(GATEWAY_ID, TartsPassiveIR::Create(SENSOR_ID, 30, 102, 2, 2))) {
        Tarts.RemoveGateway(GATEWAY_ID);
        printf("TARTs Sensor Registration Failed!\n");
        return 2;
    } else {
        printf("Registered sensor.\n");
    }

    return 0;
}

void MosqMessage_callback(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
	if(message->payloadlen){
	    printf("%s %s\n", message->topic, (char *)message->payload);
	}else{
	    printf("%s (null)\n", message->topic);
	}
	fflush(stdout);
}

void MosqConnect_callback(struct mosquitto *mosq, void *userdata, int result)
{
    if(result){
        fprintf(stderr, "Connect failed\n");
    }
}

void MosqSubscribe_callback(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	int i;

	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

void MosqLog_callback(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	/* Pring all log messages regardless of level. */
	printf("%s\n", str);
}


struct mosquitto* SetupMosquittoClient(char const* server, int port, char username[256], char password[256]) {
    static struct mosquitto *mosq = NULL;

    int iResult;
    int keepalive = 60;
    bool cleanSession = true;

    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, cleanSession, NULL);
    if ( ! mosq) {
        throw "MOSQUITTO allocation error.";
    }
    mosquitto_log_callback_set(mosq, MosqLog_callback);
    mosquitto_connect_callback_set(mosq, MosqConnect_callback);
    mosquitto_message_callback_set(mosq, MosqMessage_callback);
    mosquitto_subscribe_callback_set(mosq, MosqSubscribe_callback);

    mosquitto_tls_set(mosq, NULL, "/etc/ssl/certs", NULL, NULL,NULL);
    mosquitto_username_pw_set(mosq, username, password);

    iResult = mosquitto_connect(mosq, server, port, keepalive);
    if (iResult != MOSQ_ERR_SUCCESS) {
        printf("Unable to connect to broker. %s\n", mosquitto_strerror(iResult));
    }
  
    printf("Connected to MOSQUITTO.");

    return mosq;
}


int main(void){
    Json::Value configuration;

    std::ifstream configFile ("/home/debian/luxsit/config.json", std::ifstream::binary);
    configFile >> configuration;
    configuration = configuration["TartsGateway.cpp"];
 
    if(SetupSensor() != 0) {
        exit(1);
    }

    const char * server = configuration["mqttServer"].asCString();
    int port = configuration["mqttPort"].asInt();
    char * username =(char *) configuration["mqttUserName"].asCString();
    char * password = (char *)configuration["mqttPassword"].asCString();

    mosquittoClient = SetupMosquittoClient(server, port, username, password);
    mosquitto_loop_start(mosquittoClient);
    
    while(1){
        Tarts.Process();
        TARTS_DELAYMS(100); //Allow System Sleep to occur
    }
}
